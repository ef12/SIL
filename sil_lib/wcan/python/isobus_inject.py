"""
Inject and observe ISOBUS traffic on a WCAN bus.

A worked example of the intended Python use case: drive the bus alongside a
running simulation without any compiled extension.

    python isobus_inject.py monitor --bus isobus
    python isobus_inject.py claim   --bus isobus --source 0x80
    python isobus_inject.py send    --bus isobus --pgn 0xE600 --data 00EE00
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from wcan import Bus, Frame, WcanTimeout, build_id  # noqa: E402

PGN_REQUEST = 0xEA00
PGN_ADDRESS_CLAIM = 0xEE00
GLOBAL_ADDRESS = 255


def parse_int(text: str) -> int:
    return int(text, 0)


def parse_hex(text: str) -> bytes:
    cleaned = text.replace(" ", "").replace(":", "")
    try:
        return bytes.fromhex(cleaned)
    except ValueError:
        raise argparse.ArgumentTypeError(f"{text!r} is not valid hex") from None


def monitor(bus: Bus, duration: float | None) -> int:
    print(f"listening on {bus.name!r}, {bus.node_count()} node(s) attached")
    print(Frame.header())
    deadline = None if duration is None else time.monotonic() + duration
    count = 0
    try:
        while deadline is None or time.monotonic() < deadline:
            frame = bus.try_recv(timeout=0.25)
            if frame is not None:
                print(frame)
                count += 1
    except KeyboardInterrupt:
        pass

    stats = bus.stats()
    print(f"\nreceived {count} frames")
    print(
        f"bus carried {stats.frames} frames / {stats.bits} bits "
        f"at {stats.bitrate} bit/s, utilization {stats.utilization * 100:.1f}%"
    )
    return 0


def send(bus: Bus, pgn: int, data: bytes, priority: int, source: int,
         destination: int, count: int, interval: float) -> int:
    frame = Frame(build_id(priority, pgn, source, destination), data)
    print(Frame.header())
    for index in range(count):
        bus.send(frame)
        print(frame)
        if index + 1 < count:
            time.sleep(interval)
    return 0


def claim(bus: Bus, source: int, name: int) -> int:
    """Sends an address claim, the first thing a real ECU does."""
    payload = name.to_bytes(8, "little")
    frame = Frame(build_id(6, PGN_ADDRESS_CLAIM, source, GLOBAL_ADDRESS), payload)
    bus.send(frame)
    print(Frame.header())
    print(frame)

    # A contending claim for the same address would arrive almost immediately.
    try:
        reply = bus.recv(timeout=0.5)
        print(f"contention: {reply}")
    except WcanTimeout:
        print(f"address 0x{source:02X} not contested")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bus", default="isobus", help="bus name (default: isobus)")
    parser.add_argument("--bitrate", type=parse_int, default=0,
                        help="bit/s; 0 adopts the existing bus rate")
    sub = parser.add_subparsers(dest="command", required=True)

    monitor_parser = sub.add_parser("monitor", help="print all bus traffic")
    monitor_parser.add_argument("--duration", type=float, default=None)

    send_parser = sub.add_parser("send", help="transmit a PGN")
    send_parser.add_argument("--pgn", type=parse_int, required=True)
    send_parser.add_argument("--data", type=parse_hex, default=b"")
    send_parser.add_argument("--priority", type=parse_int, default=6)
    send_parser.add_argument("--source", type=parse_int, default=0x80)
    send_parser.add_argument("--destination", type=parse_int, default=GLOBAL_ADDRESS)
    send_parser.add_argument("--count", type=int, default=1)
    send_parser.add_argument("--interval", type=float, default=0.1)

    claim_parser = sub.add_parser("claim", help="send an address claim")
    claim_parser.add_argument("--source", type=parse_int, default=0x80)
    claim_parser.add_argument("--name", type=parse_int, default=0xA000A0A0A0A0A0A0)

    args = parser.parse_args(argv)

    try:
        with Bus(args.bus, bitrate=args.bitrate or None) as bus:
            if args.command == "monitor":
                return monitor(bus, args.duration)
            if args.command == "claim":
                return claim(bus, args.source, args.name)
            return send(bus, args.pgn, args.data, args.priority, args.source,
                        args.destination, args.count, args.interval)
    except OSError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
