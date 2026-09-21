"""RF Explorer hardware adapter used by sigann."""

from __future__ import annotations

import math
import os
import time
from typing import Callable

import pyudev
import serial.tools.list_ports

import RFExplorer

from sigann_core import AnalyzerSettings, DeviceLimits, Sweep


TARGET_VENDOR_ID = "10c4"
TARGET_PRODUCT_ID = "ea60"
BAUDRATE = 500000
DEVICE_RESPONSE_TIMEOUT = 5.0
SERIAL_POLL_INTERVAL = 0.01


class RFExplorerDevice:
    """Isolate RFExplorer library details and private-field workarounds."""

    def __init__(
        self,
        *,
        clock: Callable[[], float] = time.monotonic,
        sleep: Callable[[float], None] = time.sleep,
        port_provider=None,
        communicator_factory=None,
    ):
        self.port: str | None = None
        self.limits: DeviceLimits | None = None
        self._communicator = None
        self._clock = clock
        self._sleep = sleep
        self._port_provider = port_provider or self.candidate_ports
        self._communicator_factory = communicator_factory or self._new_communicator
        self._last_sweep_time = clock()

    @staticmethod
    def candidate_ports():
        context = pyudev.Context()
        device_nodes = {
            device.device_node
            for device in context.list_devices(subsystem="tty")
            if (
                device.get("ID_VENDOR_ID") == TARGET_VENDOR_ID
                and device.get("ID_MODEL_ID") == TARGET_PRODUCT_ID
            )
        }
        return sorted(
            (
                port
                for port in serial.tools.list_ports.comports()
                if port.device in device_nodes
            ),
            key=lambda port: port.device,
        )

    @staticmethod
    def _new_communicator(ports):
        communicator = RFExplorer.RFECommunicator()
        communicator.VerboseLevel = 0
        # ConnectPort unnecessarily requires the selected port to appear in
        # this private list. Keep this compatibility workaround in one place.
        communicator.m_arrValidCP2102Ports = ports
        return communicator

    def _wait_for(self, condition, description: str) -> None:
        deadline = self._clock() + DEVICE_RESPONSE_TIMEOUT
        while not condition():
            self._communicator.ProcessReceivedString(True)
            if self._clock() >= deadline:
                raise TimeoutError(f"Timed out waiting for {description}")
            self._sleep(SERIAL_POLL_INTERVAL)

    @staticmethod
    def _abandon(communicator) -> None:
        if communicator is None:
            return
        communicator.PortConnected = False
        communicator.RunReceiveThread = False
        acquired = False
        try:
            acquired = communicator.m_hSerialPortLock.acquire(timeout=1.0)
            if acquired and communicator.m_objSerialPort.is_open:
                communicator.m_objSerialPort.close()
        except OSError:
            pass
        finally:
            if acquired:
                communicator.m_hSerialPortLock.release()

    @classmethod
    def _close_rejected(cls, communicator) -> None:
        try:
            communicator.Close()
        finally:
            cls._abandon(communicator)

    @staticmethod
    def _normalized_min_span(communicator) -> float:
        minimum = float(communicator.MinSpanMHZ)
        # Some RFExplorer library branches report the minimum in kHz for the
        # 112-point mode and MHz otherwise.
        if math.isclose(
            minimum,
            RFExplorer.RFE_Common.CONST_RFE_MIN_SWEEP_POINTS,
        ):
            minimum /= 1000.0
        return minimum

    def connect(self) -> DeviceLimits:
        ports = self._port_provider()
        if not ports:
            raise RuntimeError("No RF Explorer USB devices found")

        for port_info in ports:
            communicator = self._communicator_factory(ports)
            self._communicator = communicator
            accepted = False
            try:
                if not communicator.ConnectPort(port_info.device, BAUDRATE):
                    continue
                self._wait_for(
                    lambda: communicator.ActiveModel
                    != RFExplorer.RFE_Common.eModel.MODEL_NONE,
                    f"model information from {port_info.device}",
                )
                if not communicator.IsAnalyzer():
                    continue

                communicator.SendCommand_RequestConfigData()
                self._wait_for(
                    lambda: (
                        communicator.StartFrequencyMHZ > 0
                        and communicator.StepFrequencyMHZ > 0
                    ),
                    f"configuration from {port_info.device}",
                )
                self.port = port_info.device
                self.limits = DeviceLimits(
                    min_frequency_mhz=float(communicator.MinFreqMHZ),
                    max_frequency_mhz=float(communicator.MaxFreqMHZ),
                    min_span_mhz=self._normalized_min_span(communicator),
                    max_span_mhz=float(communicator.MaxSpanMHZ),
                )
                accepted = True
                return self.limits
            except (OSError, TimeoutError):
                pass
            finally:
                if not accepted:
                    self._close_rejected(communicator)
                    self._communicator = None

        raise RuntimeError("No RF Explorer signal analyzer found")

    def configure(self, settings: AnalyzerSettings, reset: bool = False) -> Sweep:
        communicator = self._require_communicator()
        settings.validate(self.limits)

        if reset:
            communicator.SendCommand("r")
            self._wait_for(lambda: communicator.IsResetEvent, "analyzer reset")

        communicator.SendCommand_SweepDataPointsEx(settings.sweep_points)
        communicator.SendCommand_RequestConfigData()
        self._wait_for(
            lambda: communicator.ActiveModel
            != RFExplorer.RFE_Common.eModel.MODEL_NONE,
            "analyzer configuration",
        )
        if not communicator.IsAnalyzer():
            raise RuntimeError("Connected RF Explorer is not an analyzer")

        communicator.UpdateDeviceConfig(settings.start_mhz, settings.stop_mhz)
        self._wait_for(
            lambda: math.isclose(
                settings.start_mhz,
                communicator.StartFrequencyMHZ,
                abs_tol=0.001,
            ),
            "requested analyzer frequency",
        )
        self._wait_for(
            lambda: communicator.SweepData.Count > 0,
            "first analyzer sweep",
        )
        sweep = self._latest_sweep()
        if sweep is None:
            raise RuntimeError("Analyzer returned no sweep data")
        return sweep

    def _latest_sweep(self) -> Sweep | None:
        communicator = self._require_communicator()
        count = communicator.SweepData.Count
        if count <= 0:
            return None
        raw_sweep = communicator.SweepData.GetData(count - 1)
        frequencies = []
        amplitudes = []
        for index in range(raw_sweep.TotalDataPoints):
            frequencies.append(raw_sweep.GetFrequencyMHZ(index))
            amplitudes.append(raw_sweep.GetAmplitudeDBM(index, None, False))
        communicator.CleanSweepData()
        self._last_sweep_time = self._clock()
        return Sweep.from_sequences(
            frequencies,
            amplitudes,
            rbw_khz=float(communicator.RBW_KHZ),
        )

    def read_sweep(self) -> Sweep | None:
        communicator = self._require_communicator()
        communicator.ProcessReceivedString(True)
        return self._latest_sweep()

    def connection_problem(self, sweep_timeout: float) -> str | None:
        if self.port is None or not os.path.exists(self.port):
            return "USB serial device disappeared"
        if self._clock() - self._last_sweep_time > sweep_timeout:
            return "no sweep data received"
        return None

    def close(self, disconnected: bool = False) -> None:
        communicator = self._communicator
        self._communicator = None
        self.port = None
        if communicator is None:
            return
        if disconnected:
            self._abandon(communicator)
        else:
            try:
                communicator.Close()
            finally:
                self._abandon(communicator)

    def _require_communicator(self):
        if self._communicator is None:
            raise RuntimeError("Analyzer is not connected")
        return self._communicator
