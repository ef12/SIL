# CAN Emulator Module SDD

## 1. Purpose

The can_emulator module provides an in-memory virtual CAN bus for SIL scenarios.
It models three core behaviors required by the platform architecture:

- Arbitration based on CAN ID priority (lower ID wins)
- Deterministic tie-break for equal IDs (first submitted wins)
- Routing of selected frames to all participating nodes except sender

## 2. Scope

This module provides:

- Node registration on a virtual CAN bus
- Frame submission into a pending transmit queue
- Step-based arbitration and distribution
- Per-node receive queue retrieval

This module does not provide:

- Real-time scheduling
- CAN FD payload lengths
- Physical transport (UDP or hardware)
- PGN decoding or ISOBUS semantics

## 3. File Structure

| File | Role |
|---|---|
| sw/drivers/comms/can_emulator/inc/can_emulator.h | Public API and data model |
| sw/drivers/comms/can_emulator/src/can_emulator.c | Emulator behavior implementation |
| sw/drivers/comms/can_emulator/test/test_can_emulator.c | Unit tests |

## 4. Public API

Header: sw/drivers/comms/can_emulator/inc/can_emulator.h

Core functions:

- can_emulator_init
- can_emulator_register_node
- can_emulator_submit
- can_emulator_step
- can_emulator_receive
- can_emulator_pending_tx_count

## 5. Behavioral Notes

- A call to can_emulator_step processes at most one pending frame.
- Lower CAN ID has higher arbitration priority.
- If IDs are equal, earlier submission order wins.
- Sender does not receive its own transmitted frame.
- If any receiver queue is full during routing, step fails.

## 6. Verification

Run module and project tests with Ceedling:

```powershell
Set-Location test
ruby -S ceedling test:can_emulator
```
