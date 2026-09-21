from collections import deque
from types import SimpleNamespace
import threading

import pytest

import RFExplorer

from siggen_core import CWSettings, SweepSettings
from siggen_rfexplorer import RFExplorerGenerator


class FakeSerialPort:
    is_open = False

    def close(self):
        self.is_open = False


class FakeCommunicator:
    def __init__(self, *, generator, expansion=False):
        self.ActiveModel = RFExplorer.RFE_Common.eModel.MODEL_RFGEN
        self.ExpansionBoardActive = expansion
        self.PortConnected = False
        self.RunReceiveThread = True
        self.VerboseLevel = 0
        self.m_arrValidCP2102Ports = []
        self.m_hSerialPortLock = threading.Lock()
        self.m_objSerialPort = FakeSerialPort()
        self._generator = generator
        self.closed = False
        self.requested_configuration = False
        self.cw_calls = 0
        self.sweep_calls = 0
        self.off_calls = 0

    def ConnectPort(self, _port, _baudrate):
        self.PortConnected = True
        self.m_objSerialPort.is_open = True
        return True

    def ProcessReceivedString(self, _all_events):
        return None

    def IsGenerator(self, _check_model=False):
        return self._generator

    def SendCommand_RequestConfigData(self):
        self.requested_configuration = True

    def SendCommand_GeneratorCW(self):
        self.cw_calls += 1

    def SendCommand_GeneratorSweepFreq(self):
        self.sweep_calls += 1

    def SendCommand_GeneratorRFPowerOFF(self):
        self.off_calls += 1

    def Close(self):
        self.closed = True


def communicator_factory(*communicators):
    remaining = deque(communicators)
    return lambda _ports: remaining.popleft()


def test_connect_reports_no_usb_candidates():
    device = RFExplorerGenerator(port_provider=lambda: [])
    with pytest.raises(RuntimeError, match="No RF Explorer USB devices"):
        device.connect()


def test_connect_skips_analyzer_and_selects_generator():
    ports = [
        SimpleNamespace(device="/dev/ttyUSB2"),
        SimpleNamespace(device="/dev/ttyUSB3"),
    ]
    analyzer = FakeCommunicator(generator=False)
    generator = FakeCommunicator(generator=True)
    device = RFExplorerGenerator(
        port_provider=lambda: ports,
        communicator_factory=communicator_factory(analyzer, generator),
    )

    info = device.connect()

    assert analyzer.closed
    assert generator.requested_configuration
    assert generator.off_calls == 1
    assert info.port == "/dev/ttyUSB3"


def test_standard_generator_commands_use_library_api():
    communicator = FakeCommunicator(generator=True)
    device = RFExplorerGenerator()
    device._communicator = communicator
    device.start_cw(CWSettings(1000.0, 6))

    assert communicator.RFGenCWFrequencyMHZ == 1000.0
    assert communicator.RFGenHighPowerSwitch is True
    assert communicator.RFGenPowerLevel == 2

    device.start_sweep(SweepSettings(997.0, 2, 10, 1000.0, 500))

    assert communicator.RFGenHighPowerSwitch is False
    assert communicator.RFGenPowerLevel == 2
    assert communicator.RFGenStartFrequencyMHZ == 997.0
    assert communicator.RFGenStopFrequencyMHZ == 1007.0
    assert communicator.RFGenSweepSteps == 10
    assert communicator.RFGenStepWaitMS == 500
    assert communicator.cw_calls == 1
    assert communicator.sweep_calls == 1

    device.power_off()

    assert communicator.off_calls == 1


def test_expansion_generator_uses_dbm_power():
    communicator = FakeCommunicator(generator=True, expansion=True)
    device = RFExplorerGenerator()
    device._communicator = communicator

    device.start_cw(CWSettings(1000.0, 4))

    assert communicator.RFGenExpansionPowerDBM == -10.0
    assert communicator.cw_calls == 1
