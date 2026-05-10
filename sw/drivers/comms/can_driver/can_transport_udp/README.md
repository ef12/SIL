# CAN Transport UDP Module SDD

## 1. Purpose

The can_transport_udp module bridges shared CAN frames to UDP payloads.
It provides a deterministic wire format to move CAN traffic across UDP endpoints.

## 2. Scope

This module provides:

- Frame encode/decode to/from fixed-size UDP payload
- Transport initialization with destination endpoint
- Send and receive helpers built on udp_socket

This module does not provide:

- Arbitration or routing logic
- CAN driver node registration
- Transport protocol segmentation

## 3. Wire Format

Payload size is 13 bytes:

- Bytes 0..3: CAN ID (big-endian)
- Byte 4: DLC
- Bytes 5..12: 8-byte data field

## 4. File Structure

| File | Role |
|---|---|
| sw/drivers/comms/can_transport_udp/inc/can_transport_udp.h | Public transport API |
| sw/drivers/comms/can_transport_udp/src/can_transport_udp.c | UDP transport implementation |
| sw/drivers/comms/can_transport_udp/test/test_can_transport_udp.c | Unit tests |

## 5. Verification

Run module and project tests with Ceedling:

```powershell
Set-Location test
ruby -S ceedling test:can_transport_udp
```
