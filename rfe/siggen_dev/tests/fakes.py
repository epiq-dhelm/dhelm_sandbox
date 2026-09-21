"""Test doubles for exercising siggen without RF hardware."""

from __future__ import annotations

from collections import deque
import threading

from siggen_core import CWSettings, GeneratorInfo, SweepSettings


class FakeGenerator:
    def __init__(
        self,
        port="/dev/ttyUSB0",
        *,
        connect_error=None,
        command_error=None,
        connect_gate: threading.Event | None = None,
        command_gate: threading.Event | None = None,
    ):
        self.port = port
        self.info = GeneratorInfo(port)
        self.connect_error = connect_error
        self.command_error = command_error
        self.connect_gate = connect_gate
        self.command_gate = command_gate
        self.problem = None
        self.commands = []
        self.close_calls = []

    def connect(self):
        if self.connect_gate is not None:
            self.connect_gate.wait(timeout=2.0)
        if self.connect_error is not None:
            raise self.connect_error
        return self.info

    def _command(self, name, settings=None):
        if self.command_gate is not None:
            self.command_gate.wait(timeout=2.0)
        if self.command_error is not None:
            raise self.command_error
        self.commands.append((name, settings))

    def start_cw(self, settings: CWSettings):
        self._command("cw", settings)

    def start_sweep(self, settings: SweepSettings):
        self._command("sweep", settings)

    def power_off(self):
        self._command("off")

    def connection_problem(self):
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
