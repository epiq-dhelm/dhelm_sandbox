from collections import deque
from types import SimpleNamespace
import threading

import pytest

import RFExplorer

from sigann_rfexplorer import RFExplorerDevice


class FakeSerialPort:
    is_open = False

    def close(self):
        self.is_open = False


class FakeCommunicator:
    def __init__(self, *, analyzer, minimum_span=0.256):
        self.ActiveModel = RFExplorer.RFE_Common.eModel.MODEL_433
        self.StartFrequencyMHZ = 975.0
        self.StepFrequencyMHZ = 0.1
        self.MinFreqMHZ = 15.0
        self.MaxFreqMHZ = 2700.0
        self.MinSpanMHZ = minimum_span
        self.MaxSpanMHZ = 600.0
        self.RBW_KHZ = 100.0
        self.PortConnected = False
        self.RunReceiveThread = True
        self.VerboseLevel = 0
        self.m_arrValidCP2102Ports = []
        self.m_hSerialPortLock = threading.Lock()
        self.m_objSerialPort = FakeSerialPort()
        self._analyzer = analyzer
        self.closed = False
        self.requested_configuration = False
        self.SweepData = FakeSweepData()

    def ConnectPort(self, _port, _baudrate):
        self.PortConnected = True
        return True

    def ProcessReceivedString(self, _all_events):
        return None

    def IsAnalyzer(self):
        return self._analyzer

    def SendCommand_RequestConfigData(self):
        self.requested_configuration = True

    def Close(self):
        self.closed = True

    def CleanSweepData(self):
        self.SweepData.clear()


class FakeRawSweep:
    TotalDataPoints = 2

    @staticmethod
    def GetFrequencyMHZ(index):
        return (975.0, 1025.0)[index]

    @staticmethod
    def GetAmplitudeDBM(index, _calculator, _use_correction):
        return (-80.0, -45.0)[index]


class FakeSweepData:
    def __init__(self):
        self._sweeps = [FakeRawSweep()]

    @property
    def Count(self):
        return len(self._sweeps)

    def GetData(self, index):
        return self._sweeps[index]

    def clear(self):
        self._sweeps.clear()


def communicator_factory(*communicators):
    remaining = deque(communicators)
    return lambda _ports: remaining.popleft()


def test_connect_reports_no_candidates_without_touching_hardware():
    device = RFExplorerDevice(port_provider=lambda: [])
    with pytest.raises(RuntimeError, match="No RF Explorer USB devices"):
        device.connect()


def test_connect_skips_generator_and_selects_analyzer():
    ports = [SimpleNamespace(device="/dev/ttyUSB2"), SimpleNamespace(device="/dev/ttyUSB3")]
    generator = FakeCommunicator(analyzer=False)
    analyzer = FakeCommunicator(analyzer=True)
    device = RFExplorerDevice(
        port_provider=lambda: ports,
        communicator_factory=communicator_factory(generator, analyzer),
    )

    limits = device.connect()

    assert generator.closed
    assert analyzer.requested_configuration
    assert device.port == "/dev/ttyUSB3"
    assert limits.min_frequency_mhz == 15.0
    assert limits.max_frequency_mhz == 2700.0


def test_112_point_minimum_span_is_converted_from_khz():
    ports = [SimpleNamespace(device="/dev/ttyUSB2")]
    analyzer = FakeCommunicator(analyzer=True, minimum_span=112)
    device = RFExplorerDevice(
        port_provider=lambda: ports,
        communicator_factory=communicator_factory(analyzer),
    )

    limits = device.connect()

    assert limits.min_span_mhz == pytest.approx(0.112)


def test_latest_sweep_includes_resolution_bandwidth():
    communicator = FakeCommunicator(analyzer=True)
    device = RFExplorerDevice()
    device._communicator = communicator

    sweep = device._latest_sweep()

    assert sweep.rbw_khz == 100.0
    assert sweep.frequencies_mhz == (975.0, 1025.0)
