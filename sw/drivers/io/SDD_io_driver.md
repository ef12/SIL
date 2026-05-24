# IO Driver — Software Design Document

## 1. Purpose

The `io_driver` module provides an abstract, platform-independent IO interface for digital
and analog pins. Application code programs against this interface; concrete implementations
(SIL, MCU, etc.) are injected at init time via function pointers.

## 2. Scope

This module provides:

- Abstract `IoDriver` struct with function-pointer dispatch:
  `digital_read`, `digital_write`, `analog_read`, `analog_write`, `close`
- Data fields: `digital_pin_count`, `analog_pin_count`, `initialized`
- Dispatch functions with pre-call validation

This module does not provide:

- Any concrete implementation (see `sil_io_config` for the SIL backend)
- Pin mode multiplexing or interrupt/event callbacks
- Synchronization policy (handled by the SIL config layer)

## 3. Design

Same C vtable pattern as `can_driver`: platform code embeds `IoDriver` as the first member
of a larger struct, fills in the function pointers, and sets `initialized = true`.

## 4. File Structure

| File | Role |
|------|------|
| `sw/drivers/io/io_driver.h` | Abstract driver type and dispatch API |
| `sw/drivers/io/io_driver.c` | Dispatch implementation with validation |

## 5. Verification

IO driver dispatch is tested indirectly through `test_io.c` (io_transport_udp tests).
