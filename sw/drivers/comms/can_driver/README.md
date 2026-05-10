# CAN Driver Module SDD

## 1. Purpose

The can_driver module provides a stable CAN-facing interface for higher-level SIL modules.
In the current phase it is implemented as a thin wrapper around can_emulator.

## 2. Scope

This module provides:

- Driver initialization with bus handle and node ID
- CAN frame send and receive API
- Bus-step API for deterministic simulation loops
- Pending transmit query API

This module does not provide:

- Physical CAN device integration
- Transport protocol segmentation/reassembly
- ISOBUS PGN or application-level parsing

## 3. File Structure

| File | Role |
|---|---|
| sw/drivers/comms/can_driver/inc/can_driver.h | Public driver API |
| sw/drivers/comms/can_driver/src/can_driver.c | Emulator-backed implementation |
| sw/drivers/comms/can_driver/test/test_can_driver.c | Unit tests |

## 4. Integration Notes

- can_driver keeps a generic bus pointer in its public type.
- The current implementation casts that pointer to CanEmulator internally.
- This keeps higher layers decoupled from emulator details.

## 5. Verification

Run module and project tests with Ceedling:

```powershell
Set-Location test
ruby -S ceedling test:can_driver
```
