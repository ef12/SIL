# SIL UDP Socket — Software Design Document

## 1. Purpose

The `udp_socket` module provides a minimal, platform-abstracted UDP socket API used
by all SIL transport layers. A companion helper (`sil_config_udp_socket`) provides a
one-call init+bind+timeout sequence shared by the IO and CAN BSP modules.

## 2. Components

| Component | File(s) | Responsibility |
|-----------|---------|----------------|
| `udp_socket` | `udp_socket.h`, `udp_socket.c` | Platform UDP primitives |
| `sil_config_udp_socket` | `sil_config_udp_socket.h`, `.c` | Shared init convenience function |

## 3. UDP Socket API

### Type

```c
typedef struct {
    uintptr_t native_socket;  /* Platform socket handle */
    bool      initialized;    /* Ready-to-use flag */
} UdpSocket;
```

### Functions

| Function | Description |
|----------|-------------|
| `udp_socket_init(udp, local_port)` | Create socket, bind to port. Initializes Winsock on first call (ref-counted) |
| `udp_socket_send_to(udp, ip, port, data, len)` | Send datagram to IPv4 endpoint |
| `udp_socket_receive_from(udp, buf, len, ...)` | Receive one datagram; optional sender metadata. Returns byte count or -1 |
| `udp_socket_set_non_blocking(udp, enabled)` | Toggle non-blocking mode (`ioctlsocket` / `fcntl`) |
| `udp_socket_set_receive_timeout(udp, timeout_ms)` | Set `SO_RCVTIMEO` for blocking receives |
| `udp_socket_close(udp)` | Close socket; decrements Winsock ref count |

### Platform Details

| Platform | Implementation |
|----------|---------------|
| Windows | Winsock2 via `ws2_32.dll`. `WSAStartup`/`WSACleanup` is reference-counted across all `UdpSocket` instances |
| POSIX | Standard BSD sockets (planned, stubs only currently) |

### Error Handling

- All functions validate input (NULL checks, `initialized` guard)
- `udp_socket_init` cleans up Winsock on socket or bind failure
- `udp_socket_send_to` returns false if `sendto` sends fewer bytes than requested
- `udp_socket_receive_from` returns -1 on error or timeout

## 4. SIL Config Helper

```c
bool sil_config_udp_socket_init(UdpSocket *socket, uint16_t local_port, uint32_t timeout_ms);
```

Combines `udp_socket_init()` + optional `udp_socket_set_receive_timeout()` into a
single call. On failure, the socket is closed automatically.

Used internally by `sil_io_config` and `sil_vcan_config` — not typically called
by application code.

## 5. File Structure

| File | Role |
|------|------|
| `sil_lib/udp_socket/udp_socket.h` | Public socket API |
| `sil_lib/udp_socket/udp_socket.c` | Winsock implementation |
| `sil_lib/udp_socket/sil_config_udp_socket.h` | Init helper API |
| `sil_lib/udp_socket/sil_config_udp_socket.c` | Init helper implementation |
| `sil_lib/udp_socket/test/test_udp_socket.c` | Unit tests |

## 6. Dependencies

| Dependency | Visibility | Purpose |
|------------|------------|---------|
| `ws2_32` | Link-time (Windows) | Winsock2 library |

No source-level dependencies on other SIL modules. This is the bottom of the
dependency tree.

## 7. Verification

```powershell
Set-Location test
ruby -S ceedling test:udp_socket
```
