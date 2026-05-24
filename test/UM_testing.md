# Unit Testing — User Manual

## Overview

This project uses **Ceedling 0.31.1** (Unity + CMock + CException) for unit testing.
Tests are discovered automatically by filename convention.

## Test Files

| Test File | Module Under Test |
|-----------|-------------------|
| `sw/drivers/can_driver/test/test_can_driver.c` | can_driver |
| `sw/drivers/can_driver/can_frame/test/test_can_frame.c` | can_frame |
| `sil_lib/vcan/can_transport_udp/test/test_can_transport_udp.c` | can_transport_udp |
| `sil_lib/vcan/can_emulator/test/test_can_emulator.c` | can_emulator |
| `sil_lib/io/io_transport_udp/test/test_io_transport_udp.c` | io_transport_udp |
| `sil_lib/io/test/test_io.c` | io (sil_io_config) |
| `sil_lib/udp_socket/test/test_udp_socket.c` | udp_socket |

## Build Layout

| Purpose | Path |
|---------|------|
| Ceedling build root | `ceedling/` |
| Test executables | `ceedling/test/out/` |
| Generated mocks | `ceedling/test/mocks/` |

## Commands

Run all tests (from repository root):

```powershell
Set-Location test
ruby -S ceedling test:all
```

Run a single module's tests:

```powershell
Set-Location test
ruby -S ceedling test:can_frame
```

Clean test artifacts:

```powershell
Set-Location test
ruby -S ceedling clobber
cmake -E rm -rf ../ceedling
```

## Test Discovery

- Any file named `test_*.c` under `sw/**/test` or `sil_lib/**/test` is included automatically.
- Configuration: `test/project.yml`
- Platform libraries: `ws2_32` (Windows sockets, needed by udp_socket tests)

## Adding a New Test

1. Create `test_<module>.c` in the module's `test/` subdirectory.
2. Add test functions named `test_<behavior>(void)`.
3. Include mocks with `#include "Mock<header>.h"` when needed.
4. Run `ruby -S ceedling test:all` from the `test/` folder.
