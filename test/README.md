# Unit Testing Guide

This project uses:

- Unity for test assertions and test runner flow
- CMock for generated mocks from C headers
- Ceedling to discover, build, and run tests

## Current Test Files

| Test File | Module |
|---|---|
| sw/drivers/io/test/test_io.c | io |
| sw/drivers/io/io_transport_udp/test/test_io_transport_udp.c | io_transport_udp |
| sw/drivers/comms/can_driver/can_transport_udp/test/test_can_transport_udp.c | can_transport_udp |
| sw/drivers/comms/can_driver/can_frame/test/test_can_frame.c | can_frame |
| sw/drivers/comms/can_driver/test/test_can_driver.c | can_driver |
| sw/drivers/comms/can_driver/can_emulator/test/test_can_emulator.c | can_emulator |
| sw/drivers/comms/udp_socket/test/test_udp_socket.c | udp_socket |

Latest validation:

- 31/31 tests passed with Ceedling

## Test Build Layout

| Purpose | Path |
|---|---|
| Ceedling build root | ceedling |
| Test executables | ceedling/test/out |
| Generated mocks | ceedling/test/mocks |

## Commands

Run from repository root.

Run tests:

```powershell
Set-Location test
ruby -S ceedling test:all
```

Clean tests:

```powershell
Set-Location test
ruby -S ceedling clobber
cmake -E rm -rf ../ceedling
```

## Test Discovery Rule

- Any file named `test_*.c` under `sw/**/test` is included automatically.

## Add a New Unit Test Module

1. Create a new test file in the corresponding module path under sw/drivers/<domain>/<module>/test.
2. Add test functions named `test_<behavior>(void)`.
3. Include mocks with `#include "Mock<module>.h"` when required.
4. Run `ruby -S ceedling test:all` from the `test` folder.

## Notes

- Ceedling configuration is in `test/project.yml`.
- Include/source paths are configured to cover `sw/**/inc` and `sw/**/src`.
- Mock generation is managed by Ceedling/CMock from included headers.
