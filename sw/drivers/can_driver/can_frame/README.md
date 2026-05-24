# CAN Frame Module SDD

## 1. Purpose

The can_frame module defines the shared CAN frame data model used by communication modules.
It centralizes frame constraints and utility functions to avoid duplicated type definitions.

## 2. Scope

This module provides:

- Canonical CanFrame type
- Frame validity check (DLC range)
- Utility helpers for clearing and copying frames

This module does not provide:

- Bus arbitration
- Message routing
- Protocol-level parsing

## 3. File Structure

| File | Role |
|---|---|
| sw/drivers/comms/can_frame/inc/can_frame.h | Shared frame type and API |
| sw/drivers/comms/can_frame/src/can_frame.c | Frame utility implementation |
| sw/drivers/comms/can_frame/test/test_can_frame.c | Unit tests |

## 4. Verification

Run module and project tests with Ceedling:

```powershell
Set-Location test
ruby -S ceedling test:can_frame
```
