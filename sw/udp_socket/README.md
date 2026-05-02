# UDP Socket Module SDD

## 1. Purpose

The udp_socket module provides a small, reusable UDP communication layer for the project.
It is designed to support CAN-over-UDP style communication and future integration with higher-level CAN node and emulator modules.

## 2. Scope

This module provides:

- UDP socket initialization with local port binding
- UDP datagram send to remote IPv4 endpoint
- Optional non-blocking receive mode
- Configurable receive timeout for blocking mode
- UDP datagram receive with sender metadata (IP and port)
- Socket shutdown and resource cleanup

This module does not provide:

- CAN frame encoding/decoding
- Arbitration logic
- Reliability guarantees (UDP semantics apply)
- Broadcast routing policies

## 3. Design Goals

- Written in C, usable from both C and C++
- Small API surface with predictable behavior
- Easy to unit test on localhost
- Platform abstraction boundary for network operations

## 4. File Structure

| File | Role |
|---|---|
| sw/udp_socket/inc/udp_socket.h | Public API |
| sw/udp_socket/src/udp_socket.c | Implementation |
| sw/udp_socket/test/test_udp_socket.c | Unit tests |

## 5. Public API

Header: sw/udp_socket/inc/udp_socket.h

```c
typedef struct {
  uintptr_t native_socket;
  bool initialized;
} UdpSocket;

bool udp_socket_init(UdpSocket *udp, uint16_t local_port);

bool udp_socket_send_to(const UdpSocket *udp, const char *ip, uint16_t remote_port,
                        const void *data, size_t data_len);

bool udp_socket_set_non_blocking(UdpSocket *udp, bool enabled);

bool udp_socket_set_receive_timeout(UdpSocket *udp, uint32_t timeout_ms);

int udp_socket_receive_from(UdpSocket *udp, void *buffer, size_t buffer_len,
                            char *sender_ip, size_t sender_ip_len,
                            uint16_t *sender_port);

void udp_socket_close(UdpSocket *udp);
```

## 6. Behavioral Requirements

### 6.1 Initialization

- udp_socket_init shall return false if udp is NULL
- udp_socket_init shall bind a UDP socket to local_port
- On success, udp->initialized shall be true

### 6.2 Send Path

- udp_socket_send_to shall return false for invalid inputs
- Function shall send exactly data_len bytes or fail
- Destination shall be IPv4 text address plus UDP port

### 6.3 Receive Path

- udp_socket_receive_from shall return -1 on error
- On success, return value is received byte count
- If provided, sender_ip and sender_port shall be populated with source endpoint

### 6.4 Non-Blocking Mode

- udp_socket_set_non_blocking shall return false for NULL or non-initialized socket
- When enabled, receive operations shall return immediately if no datagram is available

### 6.5 Receive Timeout

- udp_socket_set_receive_timeout shall return false for NULL or non-initialized socket
- Timeout value is configured in milliseconds
- In blocking mode, receive shall return -1 after timeout if no datagram arrives

### 6.6 Shutdown

- udp_socket_close shall be safe to call on NULL or non-initialized socket
- On close, initialized shall be reset to false

## 7. Platform Notes

- Current functional implementation targets Windows (_WIN32) using Winsock2
- On non-Windows builds, functions currently return failure defaults
- On Windows, ws2_32 is required and linked by CMake

## 8. Threading and Concurrency

- The module does not provide internal synchronization
- A single UdpSocket instance should be owned by one thread at a time
- External locking is required for concurrent access to the same instance

## 9. Error Handling

- API uses bool and int return codes for simple call-site checks
- Detailed platform error codes are intentionally not exposed in this version
- Caller should treat false or -1 as operation failure

## 10. Usage Examples

### 10.1 C Example: Send and Receive

```c
#include "udp_socket.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  UdpSocket tx;
  UdpSocket rx;

  char rx_buffer[64] = {0};
  char sender_ip[32] = {0};
  uint16_t sender_port = 0;

  const char payload[] = "hello-can";

  if (!udp_socket_init(&tx, 7201)) {
    return 1;
  }

  if (!udp_socket_init(&rx, 7202)) {
    udp_socket_close(&tx);
    return 1;
  }

  if (!udp_socket_send_to(&tx, "127.0.0.1", 7202, payload, strlen(payload))) {
    udp_socket_close(&rx);
    udp_socket_close(&tx);
    return 1;
  }

  int n = udp_socket_receive_from(&rx, rx_buffer, sizeof(rx_buffer),
                                  sender_ip, sizeof(sender_ip), &sender_port);
  if (n > 0) {
    printf("Received %d bytes from %s:%u\n", n, sender_ip, sender_port);
  }

  udp_socket_close(&rx);
  udp_socket_close(&tx);
  return 0;
}
```

### 10.2 C++ Example: Direct C API Use

```cpp
extern "C" {
#include "udp_socket.h"
}

#include <array>
#include <cstdint>

int main() {
  UdpSocket udp{};
  const std::array<uint8_t, 3> data{0x11, 0x22, 0x33};

  if (!udp_socket_init(&udp, 7301)) {
    return 1;
  }

  bool ok = udp_socket_send_to(&udp, "127.0.0.1", 7302, data.data(), data.size());
  udp_socket_close(&udp);

  return ok ? 0 : 1;
}
```

### 10.3 C Example: Non-Blocking Poll

```c
#include "udp_socket.h"

int main(void) {
  UdpSocket rx;
  char data[64];

  if (!udp_socket_init(&rx, 7401)) {
    return 1;
  }

  if (!udp_socket_set_non_blocking(&rx, true)) {
    udp_socket_close(&rx);
    return 1;
  }

  int n = udp_socket_receive_from(&rx, data, sizeof(data), NULL, 0, NULL);
  if (n < 0) {
    /* No packet available yet (or error). */
  }

  udp_socket_close(&rx);
  return 0;
}
```

### 10.4 C Example: Blocking Receive with Timeout

```c
#include "udp_socket.h"

int main(void) {
  UdpSocket rx;
  char data[64];

  if (!udp_socket_init(&rx, 7402)) {
    return 1;
  }

  if (!udp_socket_set_non_blocking(&rx, false)) {
    udp_socket_close(&rx);
    return 1;
  }

  if (!udp_socket_set_receive_timeout(&rx, 200)) {
    udp_socket_close(&rx);
    return 1;
  }

  int n = udp_socket_receive_from(&rx, data, sizeof(data), NULL, 0, NULL);
  if (n < 0) {
    /* Timed out waiting for data. */
  }

  udp_socket_close(&rx);
  return 0;
}
```

## 11. Verification

The module is verified by unit tests in:

- sw/udp_socket/test/test_udp_socket.c

Run verification with:

```powershell
Set-Location test
ruby -S ceedling test:all
```

Current tests cover:

- Initialization and close lifecycle
- Loopback send/receive behavior
- Invalid input handling
- Non-blocking receive behavior
- Receive timeout behavior

## 12. Future Extensions

- Broadcast/multicast support where needed
- Cross-platform POSIX implementation path
- Optional error-code query API for diagnostics
