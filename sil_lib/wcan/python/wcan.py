"""
Python binding for the WCAN virtual CAN bus.

Pure ``ctypes`` against ``wcan.dll`` — no build step and no compiled extension,
so it works with any CPython on Windows.

This binding is deliberately protocol agnostic: it moves CAN frames and knows
nothing about what rides on top of them. Higher layers such as J1939 or ISOBUS
belong in the application that speaks them.

The DLL must match the interpreter's bitness. A 64-bit Python needs a 64-bit
``wcan.dll``; the C peers on the bus may be 32-bit, because processes
interoperate through the shared segment rather than through this library.

Example::

    from wcan import Bus, Frame

    with Bus("wcan0") as bus:
        bus.send(Frame(0x18EEFF80, bytes(8)))
        frame = bus.recv(timeout=1.0)
"""

from __future__ import annotations

import ctypes
import os
import sys
from ctypes import (
    POINTER,
    c_char_p,
    c_int,
    c_uint8,
    c_uint32,
    c_uint64,
    c_void_p,
)
from dataclasses import dataclass, field
from pathlib import Path

__all__ = [
    "Bus",
    "BusStats",
    "Frame",
    "WcanError",
    "WcanTimeout",
    "WcanClosed",
    "FLAG_EXTENDED",
    "FLAG_RTR",
    "FLAG_FD",
    "FLAG_BRS",
    "FLAG_ESI",
    "PACE_ADMISSION",
    "PACE_DELIVERY",
    "WORST_CASE_STUFFING",
    "load_library",
]

MAX_DATA = 64

FLAG_EXTENDED = 0x01
FLAG_RTR = 0x02
FLAG_FD = 0x04
FLAG_BRS = 0x08
FLAG_ESI = 0x10

OPEN_ECHO = 0x01
WORST_CASE_STUFFING = 0x02
PACE_ADMISSION = 0x04
PACE_DELIVERY = 0x08

_STATUS_NAMES = {
    0: "success",
    -1: "invalid argument",
    -2: "out of memory",
    -3: "bus is unavailable",
    -4: "I/O error",
    -5: "operation timed out",
    -6: "protocol error",
    -7: "connection is closed",
    -8: "already open",
    -9: "invalid bus name",
    -10: "invalid frame",
    -11: "access denied",
    -12: "too many clients",
}

_OK = 0
_ERROR_TIMEOUT = -5
_ERROR_CLOSED = -7

# 29-bit identifiers do not fit the 11-bit standard field.
_MAX_STANDARD_ID = 0x7FF
_MAX_EXTENDED_ID = 0x1FFFFFFF


class WcanError(RuntimeError):
    """A WCAN call failed."""

    def __init__(self, status: int, operation: str):
        self.status = status
        self.operation = operation
        super().__init__(
            f"{operation} failed: {_STATUS_NAMES.get(status, f'status {status}')}"
        )


class WcanTimeout(WcanError):
    """No frame arrived within the timeout."""


class WcanClosed(WcanError):
    """The socket was closed or cancelled."""


def _check(status: int, operation: str) -> None:
    if status == _OK:
        return
    if status == _ERROR_TIMEOUT:
        raise WcanTimeout(status, operation)
    if status == _ERROR_CLOSED:
        raise WcanClosed(status, operation)
    raise WcanError(status, operation)


class _CFrame(ctypes.Structure):
    """Mirrors ``wcan_frame_t``."""

    _fields_ = [
        ("can_id", c_uint32),
        ("dlc", c_uint8),
        ("flags", c_uint8),
        ("data", c_uint8 * MAX_DATA),
    ]


class _CParams(ctypes.Structure):
    """Mirrors ``wcan_params_t``."""

    _fields_ = [
        ("bitrate", c_uint32),
        ("flags", c_uint32),
        ("max_lead_us", c_uint32),
    ]


class _CStats(ctypes.Structure):
    """Mirrors ``wcan_bus_stats_t``."""

    _fields_ = [
        ("frames", c_uint64),
        ("bits", c_uint64),
        ("bitrate", c_uint32),
        ("segment_bytes", c_uint32),
        ("bus_seconds", ctypes.c_double),
        ("elapsed_seconds", ctypes.c_double),
        ("utilization", ctypes.c_double),
    ]


@dataclass(frozen=True)
class BusStats:
    """A snapshot of bus activity."""

    frames: int
    bits: int
    bitrate: int
    segment_bytes: int
    bus_seconds: float
    elapsed_seconds: float
    utilization: float


@dataclass(frozen=True)
class Frame:
    """One CAN frame.

    ``flags`` defaults to marking the frame extended whenever the identifier
    cannot fit in 11 bits.
    """

    id: int
    data: bytes = b""
    flags: int | None = field(default=None)

    def __post_init__(self):
        if self.flags is None:
            extended = self.id > _MAX_STANDARD_ID
            object.__setattr__(self, "flags", FLAG_EXTENDED if extended else 0)
        limit = (
            _MAX_EXTENDED_ID if self.flags & FLAG_EXTENDED else _MAX_STANDARD_ID
        )
        if not 0 <= self.id <= limit:
            raise ValueError(f"id 0x{self.id:X} does not fit the frame format")
        if len(self.data) > MAX_DATA:
            raise ValueError(f"payload is {len(self.data)} bytes, maximum {MAX_DATA}")
        if not self.flags & FLAG_FD and len(self.data) > 8:
            raise ValueError("payloads above 8 bytes require FLAG_FD")

    @property
    def is_extended(self) -> bool:
        return bool(self.flags & FLAG_EXTENDED)

    def __str__(self) -> str:
        width = 8 if self.is_extended else 3
        return (
            f"{self.id:0{width}X}  {self.flags:02X}  "
            f"{len(self.data):3d}  {self.data.hex(' ').upper()}"
        )

    @staticmethod
    def header() -> str:
        return f"{'ID':>8}  {'FLG':>3}  {'LEN':>3}  DATA"


_library = None


def load_library(path: str | os.PathLike | None = None):
    """Loads ``wcan.dll`` and declares its signatures.

    Searches, in order: an explicit path, ``WCAN_DLL``, the directory holding
    this module, then the usual OS search path.
    """
    global _library
    if _library is not None and path is None:
        return _library

    candidates = []
    if path is not None:
        candidates.append(Path(path))
    if os.environ.get("WCAN_DLL"):
        candidates.append(Path(os.environ["WCAN_DLL"]))
    candidates.append(Path(__file__).resolve().parent / "wcan.dll")
    candidates.append(Path("wcan.dll"))

    errors = []
    library = None
    for candidate in candidates:
        try:
            library = ctypes.CDLL(str(candidate))
            break
        except OSError as error:
            errors.append(f"  {candidate}: {error}")

    if library is None:
        bits = 64 if sys.maxsize > 2**32 else 32
        raise OSError(
            f"could not load wcan.dll for {bits}-bit Python. The DLL must match "
            f"the interpreter's bitness. Tried:\n" + "\n".join(errors)
        )

    library.wcan_handle_alloc.restype = c_void_p
    library.wcan_handle_alloc.argtypes = []
    library.wcan_handle_free.restype = None
    library.wcan_handle_free.argtypes = [c_void_p]
    library.wcan_open.restype = c_int
    library.wcan_open.argtypes = [c_void_p, c_char_p, c_uint32]
    library.wcan_open_ex.restype = c_int
    library.wcan_open_ex.argtypes = [c_void_p, c_char_p, POINTER(_CParams)]
    library.wcan_send.restype = c_int
    library.wcan_send.argtypes = [c_void_p, POINTER(_CFrame)]
    library.wcan_recv_timeout.restype = c_int
    library.wcan_recv_timeout.argtypes = [c_void_p, POINTER(_CFrame), c_uint32]
    library.wcan_cancel.restype = c_int
    library.wcan_cancel.argtypes = [c_void_p]
    library.wcan_close.restype = c_int
    library.wcan_close.argtypes = [c_void_p]
    library.wcan_bus_stats.restype = c_int
    library.wcan_bus_stats.argtypes = [c_void_p, POINTER(_CStats)]
    library.wcan_node_count.restype = c_int
    library.wcan_node_count.argtypes = [c_void_p, POINTER(c_uint32)]
    library.wcan_frame_bits.restype = c_uint32
    library.wcan_frame_bits.argtypes = [POINTER(_CFrame), c_int]
    library.wcan_abi_version.restype = c_uint32
    library.wcan_abi_version.argtypes = []
    library.wcan_segment_size.restype = c_uint32
    library.wcan_segment_size.argtypes = []

    if path is None:
        _library = library
    return library


_INFINITE = 0xFFFFFFFF


class Bus:
    """A node on a WCAN virtual CAN bus.

    Echo is off by default, matching the C API: a sender does not receive its
    own frames.
    """

    def __init__(
        self,
        name: str = "wcan0",
        *,
        bitrate: int | None = None,
        echo: bool = False,
        pacing: int = 0,
        max_lead_us: int = 0,
        library=None,
    ):
        self._library = library or load_library()
        self.name = name
        self._handle = self._library.wcan_handle_alloc()
        if not self._handle:
            raise MemoryError("could not allocate a WCAN handle")

        params = _CParams(
            bitrate=bitrate or 0,
            flags=(OPEN_ECHO if echo else 0) | pacing,
            max_lead_us=max_lead_us,
        )
        status = self._library.wcan_open_ex(
            self._handle, name.encode("ascii"), ctypes.byref(params)
        )
        if status != _OK:
            self._library.wcan_handle_free(self._handle)
            self._handle = None
            _check(status, f"open({name!r})")

    def send(self, frame: Frame) -> None:
        """Transmits one frame."""
        self._require_open()
        native = _CFrame(can_id=frame.id, dlc=len(frame.data), flags=frame.flags)
        ctypes.memmove(native.data, frame.data, len(frame.data))
        _check(self._library.wcan_send(self._handle, ctypes.byref(native)), "send")

    def recv(self, timeout: float | None = None) -> Frame:
        """Returns the next frame, raising :class:`WcanTimeout` if none arrives."""
        self._require_open()
        native = _CFrame()
        milliseconds = _INFINITE if timeout is None else max(0, int(timeout * 1000))
        _check(
            self._library.wcan_recv_timeout(
                self._handle, ctypes.byref(native), milliseconds
            ),
            "recv",
        )
        return Frame(
            id=native.can_id,
            data=bytes(native.data[: native.dlc]),
            flags=native.flags,
        )

    def try_recv(self, timeout: float | None = None) -> Frame | None:
        """Like :meth:`recv`, but returns None instead of raising on timeout."""
        try:
            return self.recv(timeout)
        except WcanTimeout:
            return None

    def drain(self) -> list[Frame]:
        """Returns every frame already queued, without blocking."""
        frames = []
        while True:
            frame = self.try_recv(0)
            if frame is None:
                return frames
            frames.append(frame)

    def cancel(self) -> None:
        """Wakes a blocked :meth:`recv` without closing the bus."""
        self._require_open()
        _check(self._library.wcan_cancel(self._handle), "cancel")

    def stats(self) -> BusStats:
        """Returns a snapshot of bus activity."""
        self._require_open()
        native = _CStats()
        _check(
            self._library.wcan_bus_stats(self._handle, ctypes.byref(native)),
            "stats",
        )
        return BusStats(
            frames=native.frames,
            bits=native.bits,
            bitrate=native.bitrate,
            segment_bytes=native.segment_bytes,
            bus_seconds=native.bus_seconds,
            elapsed_seconds=native.elapsed_seconds,
            utilization=native.utilization,
        )

    def node_count(self) -> int:
        """Returns how many nodes are currently attached."""
        self._require_open()
        count = c_uint32()
        _check(
            self._library.wcan_node_count(self._handle, ctypes.byref(count)),
            "node_count",
        )
        return count.value

    def frame_bits(self, frame: Frame, worst_case: bool = False) -> int:
        """Returns the airtime of a frame in bits, including interframe space."""
        native = _CFrame(can_id=frame.id, dlc=len(frame.data), flags=frame.flags)
        ctypes.memmove(native.data, frame.data, len(frame.data))
        return self._library.wcan_frame_bits(
            ctypes.byref(native), 1 if worst_case else 0
        )

    def close(self) -> None:
        """Detaches this node. Safe to call more than once."""
        if self._handle:
            self._library.wcan_handle_free(self._handle)
            self._handle = None

    def _require_open(self) -> None:
        if not self._handle:
            raise WcanClosed(_ERROR_CLOSED, "operation")

    def __enter__(self) -> "Bus":
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> bool:
        self.close()
        return False

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass
