# CAN Frame — Software Design Document

## 1. Purpose

The `can_frame` module defines the shared CAN frame data model used by all CAN-related modules.
It centralizes frame constraints and utility functions to avoid duplicated type definitions.

## 2. Scope

This module provides:

- Canonical `CanFrame` type (32-bit ID, 8-bit DLC, 8-byte data)
- Frame validity check (`can_frame_is_valid` — validates DLC ≤ 8)
- Utility helpers: `can_frame_clear`, `can_frame_copy`

This module does not provide:

- Bus arbitration or message routing
- Protocol-level parsing

## 3. File Structure

| File | Role |
|------|------|
| `sw/drivers/can_driver/can_frame/can_frame.h` | Shared frame type and API |
| `sw/drivers/can_driver/can_frame/can_frame.c` | Frame utility implementation |
| `sw/drivers/can_driver/can_frame/test/test_can_frame.c` | Unit tests |

## 4. Verification

```powershell
Set-Location test
ruby -S ceedling test:can_frame
```
