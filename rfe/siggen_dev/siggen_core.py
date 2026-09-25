"""Hardware-independent settings and connection management for siggen."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum, auto
import math
from queue import Empty, SimpleQueue
import threading
from typing import Callable, Protocol


MIN_FREQUENCY_MHZ = 23.438
MAX_FREQUENCY_MHZ = 6000.0
MIN_POWER_LEVEL = 0
MAX_POWER_LEVEL = 7
MIN_SWEEP_STEPS = 2
MAX_SWEEP_STEPS = 9999
MIN_STEP_TIME_MS = 0
MAX_STEP_TIME_MS = 65535

POWER_DBM_BY_LEVEL = {
    0: -42.0,
    1: -39.0,
    2: -35.0,
    3: -33.0,
    4: -10.0,
    5: -7.3,
    6: -6.0,
    7: -5.0,
}


def _parse_float(value: str, description: str) -> float:
    try:
        result = float(value)
    except ValueError as error:
        raise ValueError(f"{description} must be a number") from error
    if not math.isfinite(result):
        raise ValueError(f"{description} must be finite")
    return result


def _parse_int(value: str, description: str) -> int:
    try:
        return int(value)
    except ValueError as error:
        raise ValueError(f"{description} must be a whole number") from error


def _validate_frequency(
    frequency_mhz: float, description: str, info: GeneratorInfo | None = None
) -> None:
    minimum = info.min_frequency_mhz if info is not None else MIN_FREQUENCY_MHZ
    maximum = info.max_frequency_mhz if info is not None else MAX_FREQUENCY_MHZ
    if not minimum <= frequency_mhz <= maximum:
        raise ValueError(
            f"{description} must be between {minimum:g} and "
            f"{maximum:g} MHz"
        )


def _validate_power(power_level: int) -> None:
    if not MIN_POWER_LEVEL <= power_level <= MAX_POWER_LEVEL:
        raise ValueError(
            f"Power level must be between {MIN_POWER_LEVEL} and "
            f"{MAX_POWER_LEVEL}"
        )


@dataclass(frozen=True)
class CWSettings:
    frequency_mhz: float = 1002.0
    power_level: int = 0

    @classmethod
    def from_strings(
        cls, frequency: str, power: str, *, info: GeneratorInfo | None = None
    ) -> "CWSettings":
        settings = cls(
            _parse_float(frequency, "Frequency"),
            _parse_int(power, "Power level"),
        )
        return settings.validate(info)

    def validate(self, info: GeneratorInfo | None = None) -> "CWSettings":
        _validate_frequency(self.frequency_mhz, "Frequency", info)
        _validate_power(self.power_level)
        return self


@dataclass(frozen=True)
class SweepSettings:
    start_mhz: float = 997.0
    power_level: int = 0
    steps: int = 10
    step_khz: float = 1002.0
    step_time_ms: int = 1000

    @property
    def stop_mhz(self) -> float:
        return self.start_mhz + (self.steps * self.step_khz / 1000.0)

    @classmethod
    def from_strings(
        cls,
        start: str,
        power: str,
        steps: str,
        step_khz: str,
        step_time_ms: str,
        *,
        info: GeneratorInfo | None = None,
    ) -> "SweepSettings":
        settings = cls(
            start_mhz=_parse_float(start, "Start frequency"),
            power_level=_parse_int(power, "Power level"),
            steps=_parse_int(steps, "Number of steps"),
            step_khz=_parse_float(step_khz, "Frequency step"),
            step_time_ms=_parse_int(step_time_ms, "Step time"),
        )
        return settings.validate(info)

    def validate(self, info: GeneratorInfo | None = None) -> "SweepSettings":
        _validate_frequency(self.start_mhz, "Start frequency", info)
        _validate_frequency(self.stop_mhz, "Stop frequency", info)
        _validate_power(self.power_level)
        if not MIN_SWEEP_STEPS <= self.steps <= MAX_SWEEP_STEPS:
            raise ValueError(
                f"Number of steps must be between {MIN_SWEEP_STEPS} "
                f"and {MAX_SWEEP_STEPS}"
            )
        if self.step_khz <= 0:
            raise ValueError("Frequency step must be greater than zero")
        if not MIN_STEP_TIME_MS <= self.step_time_ms <= MAX_STEP_TIME_MS:
            raise ValueError(
                f"Step time must be between {MIN_STEP_TIME_MS} "
                f"and {MAX_STEP_TIME_MS} ms"
            )
        return self


@dataclass(frozen=True)
class GeneratorInfo:
    port: str
    expansion_active: bool = False
    min_frequency_mhz: float = MIN_FREQUENCY_MHZ
    max_frequency_mhz: float = MAX_FREQUENCY_MHZ

    def __post_init__(self) -> None:
        if not (
            math.isfinite(self.min_frequency_mhz)
            and math.isfinite(self.max_frequency_mhz)
            and 0 < self.min_frequency_mhz < self.max_frequency_mhz
        ):
            raise ValueError("Generator reported an invalid frequency range")


class GeneratorDevice(Protocol):
    port: str | None
    info: GeneratorInfo | None

    def connect(self) -> GeneratorInfo:
        """Find and connect to an RF Explorer signal generator."""

    def start_cw(self, settings: CWSettings) -> None:
        """Start continuous-wave output."""

    def start_sweep(self, settings: SweepSettings) -> None:
        """Start frequency-sweep output."""

    def power_off(self) -> None:
        """Disable RF output."""

    def connection_problem(self) -> str | None:
        """Return a human-readable connection failure or None."""

    def close(self, disconnected: bool = False) -> None:
        """Disable output when possible and release the device."""


class ConnectionState(Enum):
    DISCONNECTED = auto()
    CONNECTING = auto()
    READY = auto()
    BUSY = auto()
    STOPPING = auto()
    STOPPED = auto()


class EventKind(Enum):
    CONNECTED = auto()
    COMMAND_SUCCEEDED = auto()
    FAILED = auto()


@dataclass(frozen=True)
class ControllerEvent:
    kind: EventKind
    message: str
    info: GeneratorInfo | None = None
    output_active: bool | None = None


class GeneratorController:
    """Run blocking RF Explorer operations away from the Tk event thread."""

    def __init__(self, device_factory: Callable[[], GeneratorDevice]):
        self._device_factory = device_factory
        self._device: GeneratorDevice | None = None
        self._state = ConnectionState.DISCONNECTED
        self._events: SimpleQueue[ControllerEvent] = SimpleQueue()
        self._lock = threading.RLock()
        self._generation = 0
        self._worker: threading.Thread | None = None

    @property
    def state(self) -> ConnectionState:
        with self._lock:
            return self._state

    @property
    def info(self) -> GeneratorInfo | None:
        with self._lock:
            return self._device.info if self._device is not None else None

    def connect(self, *, reason: str = "Connecting to generator") -> bool:
        with self._lock:
            if self._state in {
                ConnectionState.CONNECTING,
                ConnectionState.BUSY,
                ConnectionState.STOPPING,
                ConnectionState.STOPPED,
            }:
                return False
            old_device = self._device
            self._device = None
            self._state = ConnectionState.CONNECTING
            self._generation += 1
            generation = self._generation

        worker = threading.Thread(
            target=self._connect_worker,
            args=(generation, old_device, reason),
            name="siggen-connect",
            daemon=True,
        )
        self._worker = worker
        worker.start()
        return True

    def _connect_worker(
        self,
        generation: int,
        old_device: GeneratorDevice | None,
        reason: str,
    ) -> None:
        if old_device is not None:
            try:
                old_device.close(disconnected=True)
            except Exception:
                pass

        device = None
        try:
            device = self._device_factory()
            info = device.connect()
        except Exception as error:
            if device is not None:
                try:
                    device.close(disconnected=True)
                except Exception:
                    pass
            with self._lock:
                if (
                    generation != self._generation
                    or self._state == ConnectionState.STOPPED
                ):
                    return
                self._state = ConnectionState.DISCONNECTED
            self._events.put(
                ControllerEvent(EventKind.FAILED, f"{reason}: {error}")
            )
            return

        with self._lock:
            stale = generation != self._generation or self._state in {
                ConnectionState.STOPPING,
                ConnectionState.STOPPED,
            }
            if not stale:
                self._device = device
                self._state = ConnectionState.READY
        if stale:
            device.close(disconnected=True)
            return
        self._events.put(
            ControllerEvent(
                EventKind.CONNECTED,
                f"Connected to signal generator on {info.port}",
                info=info,
                output_active=False,
            )
        )

    def start_cw(self, settings: CWSettings) -> bool:
        settings.validate(self.info)
        return self._start_command(
            "Starting continuous-wave output",
            lambda device: device.start_cw(settings),
            "Continuous-wave output is on",
            True,
        )

    def start_sweep(self, settings: SweepSettings) -> bool:
        settings.validate(self.info)
        return self._start_command(
            "Starting frequency sweep",
            lambda device: device.start_sweep(settings),
            "Frequency sweep is on",
            True,
        )

    def power_off(self) -> bool:
        return self._start_command(
            "Turning RF output off",
            lambda device: device.power_off(),
            "RF output is off",
            False,
        )

    def _start_command(
        self,
        name: str,
        operation: Callable[[GeneratorDevice], None],
        success_message: str,
        output_active: bool,
    ) -> bool:
        with self._lock:
            if self._state != ConnectionState.READY or self._device is None:
                return False
            device = self._device
            self._state = ConnectionState.BUSY
            self._generation += 1
            generation = self._generation

        worker = threading.Thread(
            target=self._command_worker,
            args=(
                generation,
                device,
                name,
                operation,
                success_message,
                output_active,
            ),
            name="siggen-command",
            daemon=True,
        )
        self._worker = worker
        worker.start()
        return True

    def _command_worker(
        self,
        generation: int,
        device: GeneratorDevice,
        name: str,
        operation: Callable[[GeneratorDevice], None],
        success_message: str,
        output_active: bool,
    ) -> None:
        try:
            operation(device)
        except Exception as error:
            try:
                device.close(disconnected=True)
            except Exception:
                pass
            with self._lock:
                if (
                    generation != self._generation
                    or self._state == ConnectionState.STOPPED
                ):
                    return
                self._device = None
                self._state = ConnectionState.DISCONNECTED
            self._events.put(
                ControllerEvent(EventKind.FAILED, f"{name} failed: {error}")
            )
            return

        with self._lock:
            stale = generation != self._generation or self._state in {
                ConnectionState.STOPPING,
                ConnectionState.STOPPED,
            }
            if not stale:
                self._state = ConnectionState.READY
        if stale:
            # A command may just have enabled RF output. Use the normal close
            # path so shutdown always sends the generator power-off command.
            device.close(disconnected=False)
            return
        self._events.put(
            ControllerEvent(
                EventKind.COMMAND_SUCCEEDED,
                success_message,
                info=device.info,
                output_active=output_active,
            )
        )

    def connection_problem(self) -> str | None:
        with self._lock:
            if self._state != ConnectionState.READY or self._device is None:
                return None
            device = self._device
        return device.connection_problem()

    def drain_events(self) -> list[ControllerEvent]:
        events = []
        while True:
            try:
                events.append(self._events.get_nowait())
            except Empty:
                return events

    def shutdown(self) -> None:
        with self._lock:
            if self._state == ConnectionState.STOPPED:
                return
            prior_state = self._state
            self._state = ConnectionState.STOPPING
            self._generation += 1
            device = self._device
            self._device = None

        if device is not None and prior_state != ConnectionState.BUSY:
            try:
                device.close(disconnected=False)
            except Exception:
                pass

        with self._lock:
            self._state = ConnectionState.STOPPED
        self.drain_events()
