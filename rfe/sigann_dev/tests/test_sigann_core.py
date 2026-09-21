from __future__ import annotations

import threading
import time

import pytest

from sigann_core import (
    AnalyzerController,
    AnalyzerSettings,
    ConnectionState,
    DeviceLimits,
    EventKind,
    Sweep,
)
from tests.fakes import DEFAULT_SWEEP, FakeAnalyzer, FakeFactory


def wait_for_event(controller, kind, timeout=1.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        for event in controller.drain_events():
            if event.kind == kind:
                return event
        time.sleep(0.005)
    raise AssertionError(f"Timed out waiting for {kind}")


class TestAnalyzerSettings:
    def test_default_range_and_sweep_points(self):
        settings = AnalyzerSettings()
        assert settings.start_mhz == 975.0
        assert settings.stop_mhz == 1025.0
        assert settings.sweep_points == 256

    @pytest.mark.parametrize(
        ("span", "points"),
        [(20.0, 112), (20.1, 256), (149.9, 256), (150.0, 512)],
    )
    def test_sweep_point_boundaries(self, span, points):
        assert AnalyzerSettings(span_mhz=span).sweep_points == points

    def test_device_boundaries_are_inclusive(self):
        limits = DeviceLimits(100.0, 200.0, 10.0, 100.0)
        AnalyzerSettings(center_mhz=150.0, span_mhz=100.0).validate(limits)

    @pytest.mark.parametrize(
        "settings",
        [
            AnalyzerSettings(center_mhz=0),
            AnalyzerSettings(span_mhz=0),
            AnalyzerSettings(marker_count=-1),
            AnalyzerSettings(marker_count=6),
        ],
    )
    def test_invalid_values_are_rejected(self, settings):
        with pytest.raises(ValueError):
            settings.validate()

    def test_text_parsing_has_actionable_errors(self):
        settings = AnalyzerSettings()
        with pytest.raises(ValueError, match="must be a number"):
            settings.with_span("wide")
        with pytest.raises(ValueError, match="whole number"):
            settings.with_marker_count("1.5")


def test_sweep_requires_matching_nonempty_data():
    with pytest.raises(ValueError):
        Sweep.from_sequences([], [])
    with pytest.raises(ValueError):
        Sweep.from_sequences([1.0], [2.0, 3.0])


def test_sweep_carries_resolution_bandwidth():
    sweep = Sweep.from_sequences([100.0], [-42.0], rbw_khz=25.0)
    assert sweep.rbw_khz == 25.0

    with pytest.raises(ValueError, match="RBW"):
        Sweep.from_sequences([100.0], [-42.0], rbw_khz=0)


class TestAnalyzerController:
    def test_connect_and_stream_with_fake_analyzer(self):
        device = FakeAnalyzer(port="/dev/ttyUSB7")
        controller = AnalyzerController(FakeFactory(device))

        assert controller.connect(AnalyzerSettings(), reset=True)
        event = wait_for_event(controller, EventKind.CONNECTED)

        assert controller.state == ConnectionState.STREAMING
        assert event.port == "/dev/ttyUSB7"
        assert event.sweep == DEFAULT_SWEEP
        assert device.configure_calls == [(AnalyzerSettings(), True)]
        controller.shutdown()

    def test_startup_failure_can_retry(self):
        failed = FakeAnalyzer(connect_error=RuntimeError("not attached"))
        recovered = FakeAnalyzer(port="/dev/ttyUSB4")
        controller = AnalyzerController(FakeFactory(failed, recovered))

        controller.connect(AnalyzerSettings())
        failure = wait_for_event(controller, EventKind.FAILED)
        assert "not attached" in failure.message
        assert controller.state == ConnectionState.DISCONNECTED

        controller.connect(AnalyzerSettings())
        event = wait_for_event(controller, EventKind.CONNECTED)
        assert event.port == "/dev/ttyUSB4"
        controller.shutdown()

    def test_reconnect_replaces_changed_device_node(self):
        first = FakeAnalyzer(port="/dev/ttyUSB2")
        second = FakeAnalyzer(port="/dev/ttyUSB5")
        controller = AnalyzerController(FakeFactory(first, second))
        controller.connect(AnalyzerSettings())
        wait_for_event(controller, EventKind.CONNECTED)

        first.problem = "USB serial device disappeared"
        assert "disappeared" in controller.connection_problem(3.0)
        controller.connect(AnalyzerSettings(), reason="Reconnect")
        event = wait_for_event(controller, EventKind.CONNECTED)

        assert event.port == "/dev/ttyUSB5"
        assert first.close_calls == [True]
        controller.shutdown()

    def test_reconfigure_returns_a_fresh_sweep(self):
        device = FakeAnalyzer()
        controller = AnalyzerController(FakeFactory(device))
        controller.connect(AnalyzerSettings())
        wait_for_event(controller, EventKind.CONNECTED)

        updated = AnalyzerSettings(center_mhz=900.0, span_mhz=20.0)
        assert controller.reconfigure(updated)
        event = wait_for_event(controller, EventKind.RECONFIGURED)

        assert event.sweep == DEFAULT_SWEEP
        assert controller.settings == updated
        assert controller.state == ConnectionState.STREAMING
        controller.shutdown()

    def test_sweep_reads_are_nonblocking_and_health_is_reported(self):
        device = FakeAnalyzer()
        device.sweeps.append(DEFAULT_SWEEP)
        controller = AnalyzerController(FakeFactory(device))
        controller.connect(AnalyzerSettings())
        wait_for_event(controller, EventKind.CONNECTED)

        assert controller.read_sweep() == DEFAULT_SWEEP
        assert controller.read_sweep() is None
        device.problem = "no sweep data received"
        assert controller.connection_problem(3.0) == "no sweep data received"
        controller.shutdown()

    def test_shutdown_is_idempotent_and_closes_analyzer(self):
        device = FakeAnalyzer()
        controller = AnalyzerController(FakeFactory(device))
        controller.connect(AnalyzerSettings())
        wait_for_event(controller, EventKind.CONNECTED)

        controller.shutdown()
        controller.shutdown()

        assert controller.state == ConnectionState.STOPPED
        assert device.close_calls == [False]

    def test_shutdown_during_connect_discards_late_device(self):
        gate = threading.Event()
        device = FakeAnalyzer(connect_gate=gate)
        controller = AnalyzerController(FakeFactory(device))
        controller.connect(AnalyzerSettings())
        assert controller.state == ConnectionState.CONNECTING

        controller.shutdown()
        gate.set()
        deadline = time.monotonic() + 1.0
        while not device.close_calls and time.monotonic() < deadline:
            time.sleep(0.005)

        assert controller.state == ConnectionState.STOPPED
        assert device.close_calls == [True]
        assert controller.drain_events() == []

    def test_shutdown_during_reconfigure_lets_worker_close_device(self):
        device = FakeAnalyzer()
        controller = AnalyzerController(FakeFactory(device))
        controller.connect(AnalyzerSettings())
        wait_for_event(controller, EventKind.CONNECTED)

        gate = threading.Event()
        device.configure_gate = gate
        controller.reconfigure(AnalyzerSettings(center_mhz=900.0))
        assert controller.state == ConnectionState.RECONFIGURING
        controller.shutdown()
        gate.set()

        deadline = time.monotonic() + 1.0
        while not device.close_calls and time.monotonic() < deadline:
            time.sleep(0.005)

        assert controller.state == ConnectionState.STOPPED
        assert device.close_calls == [True]
        assert controller.drain_events() == []
