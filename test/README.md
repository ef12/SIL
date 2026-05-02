# Unit Testing Guide

This project uses:

- Unity for test assertions and test runner flow
- CMock for generated mocks from C headers
- Ceedling to discover, build, and run tests

## Current Test Files

| Test File | Module |
|---|---|
| sw/greeter/test/test_greeter.c | greeter |
| sw/hello_output/test/test_hello_output.c | hello_output |
| sw/udp_socket/test/test_udp_socket.c | udp_socket |

Latest validation:

- 9/9 tests passed with Ceedling

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

1. Create a new test file in sw/<module>/test.
2. Add test functions named `test_<behavior>(void)`.
3. Include mocks with `#include "Mock<module>.h"` when required.
4. Run `ruby -S ceedling test:all` from the `test` folder.

## Notes

- Ceedling configuration is in `test/project.yml`.
- Include/source paths are configured to cover `sw/**/inc` and `sw/**/src`.
- Mock generation is managed by Ceedling/CMock from included headers.
