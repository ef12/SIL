# SIL vCAN — Software Design Document

## 1. Purpose

The SIL vCAN subsystem provides virtual CAN communication for the SIL platform.
It allows an embedded application using the abstract `CanDriver` interface to exchange
CAN frames with a remote peer over UDP, and optionally use an in-memory bus emulator
for multi-node arbitration.

## 2. Components

| Component | File(s) | Responsibility |
|-----------|---------|----------------|
| `sil_vcan_config` | `sil_vcan_config.h`, `.c` | BSP entry point: init, driver access, teardown |
| `can_transport_udp` | `can_transport_udp/can_transport_udp.h`, `.c` | CAN frame ↔ UDP serialization |
| `can_emulator` | `can_emulator/can_emulator.h`, `.c` | In-memory virtual CAN bus with arbitration |

## 3. Public API

Applications include only `sil_vcan_config.h`:

```c
#include "sil_vcan_config.h"

SilVcanConfig sil_can = {0};
SilVcanConfigParams params = {
    .local_port  = 7401,
    .remote_ip   = "127.0.0.1",
    .remote_port = 7402,
    .timeout_ms  = 1,   /* non-blocking poll */
};

sil_vcan_config_init(&sil_can, &params);
CanDriver *can = sil_vcan_config_get_driver(&sil_can);

/* Use can_driver_send(), can_driver_receive(), etc. */

sil_vcan_config_deinit(&sil_can);
```

### Functions

| Function | Description |
|----------|-------------|
| `sil_vcan_config_init()` | Opens socket with receive timeout, inits transport, wires driver |
| `sil_vcan_config_get_driver()` | Returns `CanDriver *` with wired function pointers |
| `sil_vcan_config_deinit()` | Closes socket, frees memory |

### Configuration Parameters

| Field | Type | Description |
|-------|------|-------------|
| `local_port` | `uint16_t` | UDP port to bind |
| `remote_ip` | `const char *` | Peer IPv4 address |
| `remote_port` | `uint16_t` | Peer UDP port |
| `timeout_ms` | `uint32_t` | Receive timeout (0 = blocking) |

## 4. Internal Design

### Driver Extension Pattern

`SilCanDriver` embeds `CanDriver` as its first member and adds a pointer to the
`CanTransportUdp` instance. The `send` and `receive` callbacks delegate to
`can_transport_udp_send_frame` and `can_transport_udp_receive_frame`.

### Ownership

`SilVcanInternal` is a heap-allocated struct that owns:

- `UdpSocket` — bound to `local_port` with configurable receive timeout
- `CanTransportUdp` — bound to the socket
- `SilCanDriver` — the extended driver with wired callbacks

### Receive Model

Unlike the IO subsystem, CAN uses **polled receive** — the application calls
`can_driver_receive()` in its main loop. No background thread is used.
The socket timeout controls how long each receive call blocks.

### Teardown

`sil_vcan_config_deinit()` closes the socket and frees the internal struct.

## 5. Wire Protocol

CAN frames are serialized as fixed 13-byte UDP datagrams:

| Offset | Size | Content |
|--------|------|---------|
| 0–3 | 4 | CAN ID (uint32, big-endian) |
| 4 | 1 | DLC (0–8) |
| 5–12 | 8 | Data bytes (padded to 8) |

### Validation

- Payload must be exactly `CAN_TRANSPORT_UDP_WIRE_SIZE` (13) bytes
- Decoded frame must pass `can_frame_is_valid()` (DLC ≤ 8)

### Transport API (internal)

| Function | Description |
|----------|-------------|
| `can_transport_udp_init()` | Bind socket to transport with remote endpoint |
| `can_transport_udp_encode()` | `CanFrame` → 13-byte payload |
| `can_transport_udp_decode()` | 13-byte payload → `CanFrame` |
| `can_transport_udp_send_frame()` | Encode + `sendto` |
| `can_transport_udp_receive_frame()` | `recvfrom` + decode |

## 6. CAN Emulator

The `can_emulator` module provides a deterministic in-memory virtual CAN bus for
multi-node simulation. It is available as part of the SIL library but is not used
by `sil_vcan_config` (which uses point-to-point UDP instead).

### Features

- Up to `CAN_EMULATOR_MAX_NODES` (16) registered nodes
- Up to `CAN_EMULATOR_MAX_PENDING_TX` (64) pending transmit frames
- Per-node receive queue of `CAN_EMULATOR_MAX_RX_QUEUE` (64) frames
- Deterministic arbitration: lowest CAN ID wins; sequence number tie-break
- Stepped execution: `can_emulator_step()` processes one frame per call

### API

| Function | Description |
|----------|-------------|
| `can_emulator_init()` | Zero-initialize emulator state |
| `can_emulator_register_node()` | Add a node to the virtual bus |
| `can_emulator_submit()` | Enqueue a frame for arbitration |
| `can_emulator_step()` | Arbitrate + route one winning frame to all other nodes |
| `can_emulator_receive()` | Pop one frame from a node's receive queue |
| `can_emulator_pending_count()` | Query pending transmit count |

## 7. File Structure

| File | Role |
|------|------|
| `sil_lib/vcan/sil_vcan_config.h` | Public API: types, init, get_driver, deinit |
| `sil_lib/vcan/sil_vcan_config.c` | BSP implementation: driver wiring, cleanup |
| `sil_lib/vcan/can_transport_udp/can_transport_udp.h` | Transport API (internal) |
| `sil_lib/vcan/can_transport_udp/can_transport_udp.c` | Transport implementation |
| `sil_lib/vcan/can_transport_udp/test/test_can_transport_udp.c` | Transport unit tests |
| `sil_lib/vcan/can_emulator/can_emulator.h` | Emulator API (internal) |
| `sil_lib/vcan/can_emulator/can_emulator.c` | Emulator implementation |
| `sil_lib/vcan/can_emulator/test/test_can_emulator.c` | Emulator unit tests |

## 8. Dependencies

| Dependency | Visibility | Purpose |
|------------|------------|---------|
| `can_driver` | Public | Abstract `CanDriver` type returned to application |
| `can_frame` | Public (transitive) | Shared `CanFrame` type |
| `can_transport_udp` | Private | CAN frame serialization over UDP |
| `sil_config_udp_socket` | Private | Shared socket init helper |
| `udp_socket` | Private (transitive) | Platform UDP primitives |

## 9. Verification

```powershell
Set-Location test
ruby -S ceedling test:can_transport_udp
ruby -S ceedling test:can_emulator
```
