# WCAN — Windows virtual CAN bus

A user-mode virtual CAN bus for Windows. It gives several local processes a
shared software CAN bus without CAN hardware, without a kernel driver, and
without a broker process to install or start.

Nodes meet in a named shared-memory segment. Every process is a real node, so
a simulation, a Virtual Terminal and a Python injector can all sit on the same
bus at once — including across process bitness.

This module replaces the UDP-based `vcan` subsystem, which was point-to-point
and could therefore only ever connect two peers.

## Layout

| Path | Contents |
|------|----------|
| `wcan.h` | Core types, status codes, frame flags, validation |
| `wcan_shm.h` / `wcan_shm.c` | The shared-memory transport and bit-time scheduler |
| `wcan_layout.h` | Segment layout: the cross-process ABI |
| `wcan_export.h` | Export and calling-convention control for DLL builds |
| `wcan_validate.c` | Bus name and frame validation, status descriptions |
| `test/` | Transport, bit-time and layout test programs |
| `tools/` | `wcan_peer` reference node, `wcan_resources` footprint report |
| `python/` | `ctypes` binding and an ISOBUS injection example |
| `agisostack/` | `CANHardwarePlugin` implementation for AgIsoStack |

## The segment is the ABI

Processes interoperate through the shared segment, not through the library
binary, so they need not share a compiler, a C runtime, or a bitness. Only the
segment layout must agree, and `wcan_layout.h` pins it with explicit padding
plus static assertions that fail the build on any disagreement.

Verified identical across MinGW GCC i686, MinGW GCC x86_64 and MSVC x64:
736408 bytes with every field offset matching. That is what lets a 32-bit
simulation share a bus with 64-bit Python.

## Behaviour

- Frames reach only nodes attached to the same bus name.
- Echo is off by default: a sender does not receive its own frames, matching a
  real CAN controller. It can be enabled per socket.
- Arbitration is bit-time accurate, with exact stuff-bit counting, IDE and RTR
  handling, and per-sender FIFO so transport protocol sequences stay intact.
- Bitrate is per bus. 125 k, 250 k, 500 k and 1 Mbit/s are all supported; a
  joiner either inherits the rate or is rejected for asking for another.
- A crashed node's slot is reclaimed through mutex abandonment rather than a
  heartbeat timeout, so a process paused on a breakpoint is never evicted.

## Building

The module is Windows-only and compiles into `sil_lib` automatically there. On
other platforms it is skipped.

The test programs are standalone executables:

```powershell
gcc -std=c11 -O2 test/test_wcan_transport.c -I. <path-to>/libsil_lib.a `
    -ladvapi32 -lwinmm -o test_wcan_transport.exe
```

## Known trade-offs

Any node can corrupt the segment for every other node. A broker could have
validated centrally, at the cost of a process to deploy and roughly four times
the latency. For an in-house SIL tool the trade is usually worth it, but it is
a real one.

With no daemon, the virtual bus clock only advances while some node is inside
send or receive. Every real ISOBUS node polls its bus, so this holds in
practice.
