# IO Transport UDP Module SDD

## 1. Purpose

The io_transport_udp module synchronizes application-provided IO buffers over UDP.
It is a transport adapter that can move digital and analog IO states between SIL nodes.

## 2. Scope

This module provides:

- Transport initialization with remote endpoint and buffer bindings
- Serialization and transmission of bound IO buffers
- Reception and application of remote IO payloads into bound buffers

This module does not provide:

- Business logic for IO mapping
- Retry/reliability protocol on top of UDP
- Thread management policy

## 3. Wire Format

Payload layout:

- Byte 0: 'I'
- Byte 1: 'O'
- Byte 2: protocol version
- Byte 3: digital pin count (0..255)
- Byte 4: analog pin count (0..255)
- Byte 5: reserved
- Bytes [6..]: digital values as 1 byte each (0 or non-zero)
- Remaining bytes: analog values as uint16 little-endian

## 4. File Structure

| File | Role |
|---|---|
| sw/drivers/io/io_transport_udp/inc/io_transport_udp.h | Public IO transport API |
| sw/drivers/io/io_transport_udp/src/io_transport_udp.c | UDP transport implementation |
| sw/drivers/io/io_transport_udp/test/test_io_transport_udp.c | Unit tests |

## 5. Verification

Run module and project tests with Ceedling:

```powershell
Set-Location test
ruby -S ceedling test:io_transport_udp
```
