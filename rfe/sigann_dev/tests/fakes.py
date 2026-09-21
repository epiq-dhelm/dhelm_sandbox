"""Test doubles for exercising sigann without RF hardware."""

from __future__ import annotations

from collections import deque
import threading

from sigann_core import AnalyzerSettings, DeviceLimits, Sweep


DEFAULT_LIMITS = DeviceLimits(15.0, 2700.0, 0.112, 600.0)
DEFAULT_SWEEP = Sweep.from_sequences(
    (975.0, 1000.0, 1025.0),
    (-90.0, -40.0, -95.0),
    rbw_khz=100.0,
)


class FakeAnalyzer:
    def __init__(
        self,
        port="/dev/ttyUSB0",
        *,
        limits=DEFAULT_LIMITS,
        connect_error=None,
        configure_error=None,
        connect_gate: threading.Event | None = None,
        configure_gate: threading.Event | None = None,
    ):
        self.port = port
        self.limits = limits
        self.connect_error = connect_error
        self.configure_error = configure_error
        self.connect_gate = connect_gate
        self.configure_gate = configure_gate
        self.problem = None
        self.sweeps = deque()
        self.connect_count = 0
        self.configure_calls = []
        self.close_calls = []

    def connect(self):
        self.connect_count += 1
        if self.connect_gate is not None:
            self.connect_gate.wait(timeout=2.0)
        if self.connect_error is not None:
            raise self.connect_error
        return self.limits

    def configure(self, settings: AnalyzerSettings, reset=False):
        if self.configure_gate is not None:
            self.configure_gate.wait(timeout=2.0)
        if self.configure_error is not None:
            raise self.configure_error
        self.configure_calls.append((settings, reset))
        return DEFAULT_SWEEP

    def read_sweep(self):
        return self.sweeps.popleft() if self.sweeps else None

    def connection_problem(self, _sweep_timeout):
        return self.problem

    def close(self, disconnected=False):
        self.close_calls.append(disconnected)


class FakeFactory:
    def __init__(self, *devices):
        self.devices = deque(devices)

    def __call__(self):
        if not self.devices:
            raise RuntimeError("No fake devices remaining")
        return self.devices.popleft()
