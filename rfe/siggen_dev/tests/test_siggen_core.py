from __future__ import annotations

import threading
import time

import pytest

from siggen_core import (
    ConnectionState,
    CWSettings,
    EventKind,
    GeneratorController,
    GeneratorInfo,
    SweepSettings,
)
from tests.fakes import FakeFactory, FakeGenerator


def wait_for_event(controller, kind, timeout=1.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        for event in controller.drain_events():
            if event.kind == kind:
                return event
        time.sleep(0.005)
    raise AssertionError(f"Timed out waiting for {kind}")


class TestSettings:
    def test_cw_parsing(self):
        settings = CWSettings.from_strings("1002.5", "7")
        assert settings.frequency_mhz == 1002.5
        assert settings.power_level == 7

    @pytest.mark.parametrize(
        ("frequency", "power"),
        [("nope", "0"), ("0", "0"), ("1000", "-1"), ("1000", "8")],
    )
    def test_invalid_cw_settings(self, frequency, power):
        with pytest.raises(ValueError):
            CWSettings.from_strings(frequency, power)

    def test_sweep_stop_frequency(self):
        settings = SweepSettings.from_strings("997", "3", "10", "1002", "1000")
        assert settings.stop_mhz == pytest.approx(1007.02)

    @pytest.mark.parametrize(
        "values",
        [
            ("997", "0", "1", "100", "10"),
            ("997", "0", "10", "0", "10"),
            ("997", "0", "10", "100", "-1"),
            ("9999", "0", "10", "1000", "10"),
        ],
    )
    def test_invalid_sweep_settings(self, values):
        with pytest.raises(ValueError):
            SweepSettings.from_strings(*values)

    def test_cw_uses_connected_generator_range(self):
        combo = GeneratorInfo("/dev/ttyUSB0", True, 0.1, 6000.0)
        standard = GeneratorInfo("/dev/ttyUSB1", False, 23.438, 6000.0)

        settings = CWSettings.from_strings("0.1", "0", info=combo)
        assert settings.frequency_mhz == 0.1
        with pytest.raises(ValueError, match="between 23.438 and 6000 MHz"):
            CWSettings.from_strings("0.1", "0", info=standard)

    def test_sweep_uses_connected_generator_range(self):
        combo = GeneratorInfo("/dev/ttyUSB0", True, 0.1, 6000.0)

        settings = SweepSettings.from_strings(
            "0.1", "0", "2", "100", "10", info=combo
        )
        assert settings.stop_mhz == pytest.approx(0.3)
        with pytest.raises(ValueError, match="between 0.1 and 6000 MHz"):
            SweepSettings.from_strings(
                "0.05", "0", "2", "100", "10", info=combo
            )

    def test_rejects_invalid_device_range(self):
        with pytest.raises(ValueError, match="invalid frequency range"):
            GeneratorInfo("/dev/ttyUSB0", True, 6000.0, 0.1)


class TestGeneratorController:
    def test_controller_accepts_connected_expansion_range(self):
        device = FakeGenerator()
        device.info = GeneratorInfo(device.port, True, 0.1, 6000.0)
        controller = GeneratorController(FakeFactory(device))
        controller.connect()
        wait_for_event(controller, EventKind.CONNECTED)

        settings = CWSettings(0.1, 0)
        assert controller.start_cw(settings)
        wait_for_event(controller, EventKind.COMMAND_SUCCEEDED)
        assert device.commands == [("cw", settings)]
        controller.shutdown()

    def test_controller_rejects_frequency_outside_connected_range(self):
        device = FakeGenerator()
        controller = GeneratorController(FakeFactory(device))
        controller.connect()
        wait_for_event(controller, EventKind.CONNECTED)

        with pytest.raises(ValueError, match="between 23.438 and 6000 MHz"):
            controller.start_cw(CWSettings(0.1, 0))
        assert device.commands == []
        controller.shutdown()

    def test_connect_and_start_cw(self):
        device = FakeGenerator(port="/dev/ttyUSB7")
        controller = GeneratorController(FakeFactory(device))
        assert controller.connect()
        event = wait_for_event(controller, EventKind.CONNECTED)
        assert event.info.port == "/dev/ttyUSB7"

        settings = CWSettings(1000.0, 4)
        assert controller.start_cw(settings)
        event = wait_for_event(controller, EventKind.COMMAND_SUCCEEDED)
        assert event.output_active is True
        assert device.commands == [("cw", settings)]
        controller.shutdown()

    def test_sweep_and_power_off(self):
        device = FakeGenerator()
        controller = GeneratorController(FakeFactory(device))
        controller.connect()
        wait_for_event(controller, EventKind.CONNECTED)

        settings = SweepSettings()
        assert controller.start_sweep(settings)
        wait_for_event(controller, EventKind.COMMAND_SUCCEEDED)
        assert controller.power_off()
        event = wait_for_event(controller, EventKind.COMMAND_SUCCEEDED)

        assert event.output_active is False
        assert device.commands == [("sweep", settings), ("off", None)]
        controller.shutdown()

    def test_startup_failure_can_retry_on_a_new_port(self):
        failed = FakeGenerator(connect_error=RuntimeError("not attached"))
        recovered = FakeGenerator(port="/dev/ttyUSB5")
        controller = GeneratorController(FakeFactory(failed, recovered))
        controller.connect()
        failure = wait_for_event(controller, EventKind.FAILED)
        assert "not attached" in failure.message

        controller.connect()
        event = wait_for_event(controller, EventKind.CONNECTED)
        assert event.info.port == "/dev/ttyUSB5"
        controller.shutdown()

    def test_health_problem_is_reported(self):
        device = FakeGenerator()
        controller = GeneratorController(FakeFactory(device))
        controller.connect()
        wait_for_event(controller, EventKind.CONNECTED)
        device.problem = "USB serial device disappeared"
        assert controller.connection_problem() == device.problem
        controller.shutdown()

    def test_command_failure_disconnects(self):
        device = FakeGenerator(command_error=OSError("write failed"))
        controller = GeneratorController(FakeFactory(device))
        controller.connect()
        wait_for_event(controller, EventKind.CONNECTED)
        controller.start_cw(CWSettings())
        event = wait_for_event(controller, EventKind.FAILED)
        assert "write failed" in event.message
        assert controller.state == ConnectionState.DISCONNECTED
        assert device.close_calls == [True]

    def test_shutdown_powers_off_connected_device(self):
        device = FakeGenerator()
        controller = GeneratorController(FakeFactory(device))
        controller.connect()
        wait_for_event(controller, EventKind.CONNECTED)
        controller.shutdown()
        controller.shutdown()
        assert controller.state == ConnectionState.STOPPED
        assert device.close_calls == [False]

    def test_shutdown_during_command_closes_after_command(self):
        gate = threading.Event()
        device = FakeGenerator(command_gate=gate)
        controller = GeneratorController(FakeFactory(device))
        controller.connect()
        wait_for_event(controller, EventKind.CONNECTED)
        controller.start_cw(CWSettings())
        controller.shutdown()
        gate.set()

        deadline = time.monotonic() + 1.0
        while not device.close_calls and time.monotonic() < deadline:
            time.sleep(0.005)
        assert device.close_calls == [False]
        assert controller.drain_events() == []
