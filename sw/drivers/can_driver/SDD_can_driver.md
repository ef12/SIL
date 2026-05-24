# CAN Driver — Software Design Document

## 1. Purpose

The `can_driver` module provides an abstract, platform-independent CAN interface.
Application code programs against this interface; concrete implementations (SIL, MCU, etc.)
are injected at init time via function pointers.

## 2. Scope

This module provides:

- Abstract `CanDriver` struct with function-pointer dispatch (`send`, `receive`, `close`)
- Dispatch functions: `can_driver_send()`, `can_driver_receive()`, `can_driver_close()`
- Pre-call validation (NULL checks, `initialized` guard)

This module does not provide:

- Any concrete implementation (see `sil_vcan_config` for the SIL backend)
- Transport protocol segmentation or reassembly
- ISOBUS PGN or application-level parsing

## 3. Design

The driver uses a C vtable pattern: platform code embeds `CanDriver` as the first member
of a larger struct, fills in the function pointers, and sets `initialized = true`.
Application code calls the dispatch functions which delegate through the pointers.

## 4. File Structure

| File | Role |
|------|------|
| `sw/drivers/can_driver/can_driver.h` | Abstract driver type and dispatch API |
| `sw/drivers/can_driver/can_driver.c` | Dispatch implementation with validation |
| `sw/drivers/can_driver/test/test_can_driver.c` | Unit tests |

## 5. Dependencies

- `can_frame` — shared `CanFrame` type

## 6. Verification

```powershell
Set-Location test
ruby -S ceedling test:can_driver
```
