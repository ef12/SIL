# IO Driver Module SDD

## 1. Purpose

The io module provides a simple hardware-abstraction style API for digital and analog IO.
In the current SIL phase, the backend is an in-memory implementation for deterministic tests.

## 2. Scope

This module provides:

- Instance-based io_init with caller-provided storage buffers
- Digital write/read by pin index
- Analog write/read by pin index

This module does not provide:

- Real hardware access
- Interrupt/event callbacks
- Pin mode multiplexing

## 3. Behavior

- IO functions return false before io_init succeeds for the given driver instance.
- Pin resources are injected by the caller at initialization time (no fixed internal resource limits).
- Digital pins support boolean values.
- Analog pins store uint16_t values.
- Invalid pin indices or NULL output pointers are rejected.

## 4. File Structure

| File | Role |
|---|---|
| sw/drivers/io/inc/io.h | Public IO driver API |
| sw/drivers/io/src/io.c | In-memory SIL implementation |
| sw/drivers/io/test/test_io.c | Unit tests |

## 5. Verification

Run module and project tests with Ceedling:

```powershell
Set-Location test
ruby -S ceedling test:io
```
