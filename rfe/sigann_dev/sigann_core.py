"""Hardware-independent state and connection management for sigann."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum, auto
import math
from queue import Empty, SimpleQueue
import threading
from typing import Callable, Protocol, Sequence


MAX_MARKERS = 5


@dataclass(frozen=True)
class DeviceLimits:
    min_frequency_mhz: float
    max_frequency_mhz: float
    min_span_mhz: float
    max_span_mhz: float


@dataclass(frozen=True)
class AnalyzerSettings:
    center_mhz: float = 1000.0
    span_mhz: float = 50.0
    marker_count: int = 0

    @property
    def start_mhz(self) -> float:
        return self.center_mhz - self.span_mhz / 2.0

    @property
    def stop_mhz(self) -> float:
        return self.center_mhz + self.span_mhz / 2.0

    @property
    def sweep_points(self) -> int:
        if self.span_mhz <= 20:
            return 112
        if self.span_mhz >= 150:
            return 512
        return 256

    def validate(self, limits: DeviceLimits | None = None) -> "AnalyzerSettings":
        values = (self.center_mhz, self.span_mhz)
        if not all(math.isfinite(value) for value in values):
            raise ValueError("Frequency and span must be finite numbers")
        if self.center_mhz <= 0:
            raise ValueError("Center frequency must be greater than zero")
        if self.span_mhz <= 0:
            raise ValueError("Span must be greater than zero")
        if not 0 <= self.marker_count <= MAX_MARKERS:
            raise ValueError(f"Marker count must be between 0 and {MAX_MARKERS}")

        if limits is not None:
            tolerance = 1e-6
            if self.span_mhz < limits.min_span_mhz - tolerance:
                raise ValueError(
                    f"Span must be at least {limits.min_span_mhz:g} MHz"
                )
            if self.span_mhz > limits.max_span_mhz + tolerance:
                raise ValueError(
                    f"Span must not exceed {limits.max_span_mhz:g} MHz"
                )
            if self.start_mhz < limits.min_frequency_mhz - tolerance:
                raise ValueError(
                    f"Start frequency must be at least "
                    f"{limits.min_frequency_mhz:g} MHz"
                )
            if self.stop_mhz > limits.max_frequency_mhz + tolerance:
                raise ValueError(
                    f"Stop frequency must not exceed "
                    f"{limits.max_frequency_mhz:g} MHz"
                )

        return self

    def with_center(self, value: str) -> "AnalyzerSettings":
        try:
            center = float(value)
        except ValueError as error:
            raise ValueError("Center frequency must be a number") from error
        return AnalyzerSettings(center, self.span_mhz, self.marker_count)

    def with_span(self, value: str) -> "AnalyzerSettings":
        try:
            span = float(value)
        except ValueError as error:
            raise ValueError("Span must be a number") from error
        return AnalyzerSettings(self.center_mhz, span, self.marker_count)

    def with_marker_count(self, value: str) -> "AnalyzerSettings":
        try:
            markers = int(value)
        except ValueError as error:
            raise ValueError("Marker count must be a whole number") from error
        return AnalyzerSettings(self.center_mhz, self.span_mhz, markers)


@dataclass(frozen=True)
class Sweep:
    frequencies_mhz: tuple[float, ...]
    amplitudes_dbm: tuple[float, ...]
    rbw_khz: float | None = None

    @classmethod
    def from_sequences(
        cls,
        frequencies_mhz: Sequence[float],
        amplitudes_dbm: Sequence[float],
        *,
        rbw_khz: float | None = None,
    ) -> "Sweep":
        frequencies = tuple(frequencies_mhz)
        amplitudes = tuple(amplitudes_dbm)
        if not frequencies or len(frequencies) != len(amplitudes):
            raise ValueError("A sweep must contain matching frequency and amplitude data")
        if rbw_khz is not None and (not math.isfinite(rbw_khz) or rbw_khz <= 0):
            raise ValueError("RBW must be a positive finite number")
        return cls(frequencies, amplitudes, rbw_khz)


class AnalyzerDevice(Protocol):
    port: str | None
    limits: DeviceLimits | None

    def connect(self) -> DeviceLimits:
        """Find and connect to an analyzer."""

    def configure(self, settings: AnalyzerSettings, reset: bool = False) -> Sweep:
        """Apply settings and return the first resulting sweep."""

    def read_sweep(self) -> Sweep | None:
        """Return the latest available sweep without blocking."""

    def connection_problem(self, sweep_timeout: float) -> str | None:
        """Return a human-readable health failure or None."""

    def close(self, disconnected: bool = False) -> None:
        """Release the device and its receive thread."""


class ConnectionState(Enum):
    DISCONNECTED = auto()
    CONNECTING = auto()
    STREAMING = auto()
    RECONFIGURING = auto()
    STOPPING = auto()
    STOPPED = auto()


class EventKind(Enum):
    CONNECTED = auto()
    RECONFIGURED = auto()
    FAILED = auto()


@dataclass(frozen=True)
class ControllerEvent:
    kind: EventKind
    message: str
    port: str | None = None
    sweep: Sweep | None = None
    limits: DeviceLimits | None = None


class AnalyzerController:
    """Own an analyzer and keep blocking connection work off the UI thread."""

    def __init__(self, device_factory: Callable[[], AnalyzerDevice]):
        self._device_factory = device_factory
        self._device: AnalyzerDevice | None = None
        self._state = ConnectionState.DISCONNECTED
        self._settings = AnalyzerSettings()
        self._events: SimpleQueue[ControllerEvent] = SimpleQueue()
        self._lock = threading.RLock()
        self._generation = 0
        self._worker: threading.Thread | None = None

    @property
    def state(self) -> ConnectionState:
        with self._lock:
            return self._state

    @property
    def settings(self) -> AnalyzerSettings:
        with self._lock:
            return self._settings

    @property
    def port(self) -> str | None:
        with self._lock:
            return self._device.port if self._device is not None else None

    @property
    def limits(self) -> DeviceLimits | None:
        with self._lock:
            return self._device.limits if self._device is not None else None

    def connect(
        self,
        settings: AnalyzerSettings,
        *,
        reset: bool = False,
        reason: str = "Connecting to analyzer",
    ) -> bool:
        settings.validate()
        with self._lock:
            if self._state in {
                ConnectionState.CONNECTING,
                ConnectionState.RECONFIGURING,
                ConnectionState.STOPPING,
                ConnectionState.STOPPED,
            }:
                return False
            old_device = self._device
            self._device = None
            self._settings = settings
            self._state = ConnectionState.CONNECTING
            self._generation += 1
            generation = self._generation

        worker = threading.Thread(
            target=self._connect_worker,
            args=(generation, old_device, settings, reset, reason),
            name="sigann-connect",
            daemon=True,
        )
        self._worker = worker
        worker.start()
        return True

    def _connect_worker(
        self,
        generation: int,
        old_device: AnalyzerDevice | None,
        settings: AnalyzerSettings,
        reset: bool,
        reason: str,
    ) -> None:
        if old_device is not None:
            try:
                old_device.close(disconnected=True)
            except Exception:
                # A failed USB endpoint may also fail during cleanup. A fresh
                # device object must still get an opportunity to reconnect.
                pass

        device = None
        try:
            device = self._device_factory()
            limits = device.connect()
            settings.validate(limits)
            sweep = device.configure(settings, reset=reset)
        except Exception as error:
            if device is not None:
                try:
                    device.close(disconnected=True)
                except Exception:
                    pass
            with self._lock:
                if generation != self._generation or self._state == ConnectionState.STOPPED:
                    return
                self._state = ConnectionState.DISCONNECTED
            self._events.put(
                ControllerEvent(EventKind.FAILED, f"{reason}: {error}")
            )
            return

        with self._lock:
            if generation != self._generation or self._state in {
                ConnectionState.STOPPING,
                ConnectionState.STOPPED,
            }:
                device.close(disconnected=True)
                return
            self._device = device
            self._settings = settings
            self._state = ConnectionState.STREAMING

        self._events.put(
            ControllerEvent(
                EventKind.CONNECTED,
                f"Connected to analyzer on {device.port}",
                port=device.port,
                sweep=sweep,
                limits=limits,
            )
        )

    def reconfigure(self, settings: AnalyzerSettings) -> bool:
        with self._lock:
            if self._state != ConnectionState.STREAMING or self._device is None:
                return False
            settings.validate(self._device.limits)
            device = self._device
            self._settings = settings
            self._state = ConnectionState.RECONFIGURING
            self._generation += 1
            generation = self._generation

        worker = threading.Thread(
            target=self._configure_worker,
            args=(generation, device, settings),
            name="sigann-configure",
            daemon=True,
        )
        self._worker = worker
        worker.start()
        return True

    def _configure_worker(
        self,
        generation: int,
        device: AnalyzerDevice,
        settings: AnalyzerSettings,
    ) -> None:
        try:
            sweep = device.configure(settings)
        except Exception as error:
            device.close(disconnected=True)
            with self._lock:
                if generation != self._generation or self._state == ConnectionState.STOPPED:
                    return
                self._device = None
                self._state = ConnectionState.DISCONNECTED
            self._events.put(
                ControllerEvent(EventKind.FAILED, f"Configuration failed: {error}")
            )
            return

        with self._lock:
            stale = generation != self._generation or self._state in {
                ConnectionState.STOPPING,
                ConnectionState.STOPPED,
            }
            if not stale:
                self._state = ConnectionState.STREAMING

        if stale:
            device.close(disconnected=True)
            return

        self._events.put(
            ControllerEvent(
                EventKind.RECONFIGURED,
                "Analyzer configuration updated",
                port=device.port,
                sweep=sweep,
                limits=device.limits,
            )
        )

    def read_sweep(self) -> Sweep | None:
        with self._lock:
            if self._state != ConnectionState.STREAMING:
                return None
            device = self._device
        return device.read_sweep() if device is not None else None

    def connection_problem(self, sweep_timeout: float) -> str | None:
        with self._lock:
            if self._state != ConnectionState.STREAMING:
                return None
            device = self._device
        if device is None:
            return "analyzer is unavailable"
        return device.connection_problem(sweep_timeout)

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
            previous_state = self._state
            self._state = ConnectionState.STOPPING
            self._generation += 1
            # A reconfiguration worker currently owns its device. It observes
            # the generation change and closes that device when it unwinds.
            device = (
                self._device
                if previous_state == ConnectionState.STREAMING
                else None
            )
            self._device = None

        if device is not None:
            try:
                device.close()
            except Exception:
                pass
            finally:
                with self._lock:
                    self._state = ConnectionState.STOPPED
        else:
            with self._lock:
                self._state = ConnectionState.STOPPED
