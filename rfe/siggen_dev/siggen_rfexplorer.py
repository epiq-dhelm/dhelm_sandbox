"""RF Explorer signal-generator hardware adapter."""

from __future__ import annotations

import os
import time
from typing import Callable

import pyudev
import serial.tools.list_ports

import RFExplorer

from siggen_core import (
    CWSettings,
    GeneratorInfo,
    POWER_DBM_BY_LEVEL,
    SweepSettings,
)


TARGET_VENDOR_ID = "10c4"
TARGET_PRODUCT_ID = "ea60"
BAUDRATE = 500000
DEVICE_RESPONSE_TIMEOUT = 5.0
SERIAL_POLL_INTERVAL = 0.01


class RFExplorerGenerator:
    """Identify and control one RF Explorer signal generator."""

    def __init__(
        self,
        *,
        clock: Callable[[], float] = time.monotonic,
        sleep: Callable[[float], None] = time.sleep,
        port_provider=None,
        communicator_factory=None,
    ):
        self.port: str | None = None
        self.info: GeneratorInfo | None = None
        self._communicator = None
        self._clock = clock
        self._sleep = sleep
        self._port_provider = port_provider or self.candidate_ports
        self._communicator_factory = communicator_factory or self._new_communicator

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
        # ConnectPort requires the selected port in this private collection.
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

    def connect(self) -> GeneratorInfo:
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
                communicator.SendCommand_RequestConfigData()
                self._wait_for(
                    lambda: communicator.ActiveModel
                    != RFExplorer.RFE_Common.eModel.MODEL_NONE,
                    f"model information from {port_info.device}",
                )
                if not communicator.IsGenerator(True):
                    continue

                # Reconnection must start from a known safe state. The RF
                # generator can continue its prior output after a USB outage.
                communicator.SendCommand_GeneratorRFPowerOFF()
                self.port = port_info.device
                self.info = GeneratorInfo(
                    port=self.port,
                    expansion_active=bool(communicator.ExpansionBoardActive),
                )
                accepted = True
                return self.info
            except (OSError, TimeoutError):
                pass
            finally:
                if not accepted:
                    self._close_rejected(communicator)
                    self._communicator = None

        raise RuntimeError("No RF Explorer signal generator found")

    @staticmethod
    def _set_power(communicator, power_level: int) -> None:
        if communicator.ExpansionBoardActive:
            communicator.RFGenExpansionPowerDBM = POWER_DBM_BY_LEVEL[power_level]
        else:
            communicator.RFGenHighPowerSwitch = power_level > 3
            communicator.RFGenPowerLevel = power_level % 4

    def start_cw(self, settings: CWSettings) -> None:
        settings.validate()
        communicator = self._require_communicator()
        communicator.RFGenCWFrequencyMHZ = settings.frequency_mhz
        self._set_power(communicator, settings.power_level)
        communicator.SendCommand_GeneratorCW()

    def start_sweep(self, settings: SweepSettings) -> None:
        settings.validate()
        communicator = self._require_communicator()
        communicator.RFGenStartFrequencyMHZ = settings.start_mhz
        communicator.RFGenStopFrequencyMHZ = settings.stop_mhz
        communicator.RFGenSweepSteps = settings.steps
        communicator.RFGenStepWaitMS = settings.step_time_ms
        self._set_power(communicator, settings.power_level)
        communicator.SendCommand_GeneratorSweepFreq()

    def power_off(self) -> None:
        self._require_communicator().SendCommand_GeneratorRFPowerOFF()

    def connection_problem(self) -> str | None:
        if self.port is None or not os.path.exists(self.port):
            return "USB serial device disappeared"
        communicator = self._communicator
        if communicator is None or not communicator.PortConnected:
            return "serial connection closed"
        return None

    def close(self, disconnected: bool = False) -> None:
        communicator = self._communicator
        self._communicator = None
        self.port = None
        self.info = None
        if communicator is None:
            return
        if disconnected:
            self._abandon(communicator)
            return
        try:
            communicator.SendCommand_GeneratorRFPowerOFF()
            communicator.Close()
        finally:
            self._abandon(communicator)

    def _require_communicator(self):
        if self._communicator is None:
            raise RuntimeError("Signal generator is not connected")
        return self._communicator
