# SIL IO — Software Design Document

## 1. Purpose

The SIL IO subsystem provides virtual IO for the SIL platform. It allows an embedded
application using the abstract `IoDriver` interface to exchange digital and analog pin
states with a remote peer (typically the Python GUI) over UDP.

## 2. Components

| Component | File(s) | Responsibility |
|-----------|---------|----------------|
| `sil_io_config` | `sil_io_config.h`, `sil_io_config.c` | BSP entry point: init, driver access, teardown |
| `io_transport_udp` | `io_transport_udp/io_transport_udp.h`, `.c` | Binary serialization of IO buffers over UDP |

## 3. Public API

Applications include only `sil_io_config.h`:

```c
#include "sil_io_config.h"

SilIoConfig sil_io = {0};
SilIoConfigParams params = {
    .local_port        = 7501,
    .remote_ip         = "127.0.0.1",
    .remote_port       = 7502,
    .sync_interval_ms  = 10,
    .digital_pin_count = 1,
    .analog_pin_count  = 2,
};

sil_io_config_init(&sil_io, &params);
IoDriver *io = sil_io_config_get_driver(&sil_io);

/* Use io_driver_digital_read(), io_driver_analog_write(), etc. */

sil_io_config_deinit(&sil_io);
```

### Functions

| Function | Description |
|----------|-------------|
| `sil_io_config_init()` | Allocates buffers, opens socket (1 ms timeout), inits transport, starts sync thread |
| `sil_io_config_get_driver()` | Returns `IoDriver *` with wired function pointers |
| `sil_io_config_deinit()` | Stops sync thread, closes socket, frees all memory |

### Configuration Parameters

| Field | Type | Description |
|-------|------|-------------|
| `local_port` | `uint16_t` | UDP port to bind |
| `remote_ip` | `const char *` | Peer IPv4 address |
| `remote_port` | `uint16_t` | Peer UDP port |
| `sync_interval_ms` | `uint32_t` | Sync thread cycle period |
| `digital_pin_count` | `uint16_t` | Number of digital pins |
| `analog_pin_count` | `uint16_t` | Number of analog pins |

## 4. Internal Design

### Driver Extension Pattern

`SilIoDriver` embeds `IoDriver` as its first member (C vtable pattern) and adds a
pointer to the `IoTransportUdp` instance. The five driver callbacks
(`digital_read`, `digital_write`, `analog_read`, `analog_write`, `close`) delegate
to `io_transport_udp_*_get/set` through this pointer.

### Ownership

`SilIoInternal` is a heap-allocated struct that owns:

- `UdpSocket` — bound to `local_port` with 1 ms receive timeout
- `IoTransportUdp` — bound to the socket and IO buffers
- `bool *digital_buf` — heap-allocated digital pin array
- `uint16_t *analog_buf` — heap-allocated analog pin array
- `SilIoDriver` — the extended driver with wired callbacks
- Thread handle — platform-specific (`HANDLE` or `pthread_t`)

### Sync Thread

A background thread runs continuously while `running == true`:

```
loop:
    io_transport_udp_receive()   ← pull remote pin state into buffers
    io_transport_udp_send()      ← push local pin state to remote
    sleep(sync_interval_ms)
```

The receive timeout is hardcoded to 1 ms to keep the thread responsive.
Application code reads/writes pins via driver callbacks at any time — the sync
thread handles the UDP exchange independently.

### Teardown

`sil_io_config_deinit()` sets `running = false`, joins the thread, closes the
socket, and frees all heap memory.

## 5. Wire Protocol

The transport serializes IO state into a compact binary payload:

| Offset | Size | Content |
|--------|------|---------|
| 0–1 | 2 | Magic: `'I'`, `'O'` |
| 2 | 1 | Protocol version (1) |
| 3 | 1 | Digital pin count (0–255) |
| 4 | 1 | Analog pin count (0–255) |
| 5 | 1 | Reserved (0) |
| 6.. | 1 × dig_count | Digital values (0x00 or 0x01) |
| .. | 2 × ana_count | Analog values (uint16, little-endian) |

Total payload size: `6 + digital_count + 2 × analog_count` bytes.

### Validation

- Magic bytes must be `'I'`, `'O'`
- Version must match `IO_TRANSPORT_UDP_VERSION` (1)
- Pin counts in header must match the transport's configured counts
- Payload length must exactly match expected size

### Transport API (internal)

| Function | Description |
|----------|-------------|
| `io_transport_udp_init()` | Bind socket + buffers to transport instance |
| `io_transport_udp_send()` | Serialize bound buffers and `sendto` |
| `io_transport_udp_receive()` | `recvfrom` and deserialize into bound buffers |
| `io_transport_udp_digital_get/set()` | Direct buffer access by pin index |
| `io_transport_udp_analog_get/set()` | Direct buffer access by pin index |
| `io_transport_udp_wire_size()` | Calculate payload size for given pin counts |

## 6. File Structure

| File | Role |
|------|------|
| `sil_lib/io/sil_io_config.h` | Public API: types, init, get_driver, deinit |
| `sil_lib/io/sil_io_config.c` | BSP implementation: driver, thread, cleanup |
| `sil_lib/io/io_transport_udp/io_transport_udp.h` | Transport API (internal to sil_lib) |
| `sil_lib/io/io_transport_udp/io_transport_udp.c` | Transport implementation |
| `sil_lib/io/io_transport_udp/test/test_io_transport_udp.c` | Transport unit tests |
| `sil_lib/io/test/test_io.c` | IO config unit tests |

## 7. Dependencies

| Dependency | Visibility | Purpose |
|------------|------------|---------|
| `io_driver` | Public | Abstract `IoDriver` type returned to application |
| `io_transport_udp` | Private | Binary serialization over UDP |
| `sil_config_udp_socket` | Private | Shared socket init helper |
| `udp_socket` | Private (transitive) | Platform UDP primitives |

## 8. Verification

```powershell
Set-Location test
ruby -S ceedling test:io_transport_udp
ruby -S ceedling test:io
```
