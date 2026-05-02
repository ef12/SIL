# Unit Testing Guide

This module uses:

- Unity as the unit test framework
- CMock for auto-generated mocks from C headers
- CMake + CTest to configure, build, and run tests

Current test targets:

- `greeter_tests` in `sw/greeter/test/test_greeter.c`
- `hello_output_tests` in `sw/hello_output/test/test_hello_output.c`

## Recent Setup Summary

- Unit tests are now wired through a reusable CMake helper: `add_unity_cmock_test(...)`.
- The helper centralizes mock generation, test executable creation, include directories, output layout, and `add_test(...)` registration.
- This keeps new test onboarding simple: add a new test source file and one helper call in `CMakeLists.txt`.
- Test/production artifacts are separated from build metadata via `ARTIFACTS_ROOT`.

Most recent validation run:

- Configure/build/test command completed successfully with Ninja.
- `ctest` result: `1/1 tests passed` (`greeter_unit_tests`).

## Adding a New Unit Test File

1. Create a new test file under `sw/<module>/test/`.
2. Add one `add_unity_cmock_test(...)` call in `CMakeLists.txt` with:
	- `TARGET` unique executable name
	- `TEST_FILE` path to your new test file
	- `TEST_NAME` ctest name
	- `OUTPUT_PATH` artifact folder under `sw/`
	- `SOURCES` production `.c` files under test
	- `INCLUDE_DIRS` module include paths
	- `MOCK_HEADERS` headers to mock with CMock
3. Reconfigure and build tests, then run `ctest`.

## Terminal Commands

Run from project root.

Configure production build tree (Ninja + split output):

```powershell
cmake -S . -B build/meta-prod -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_PRODUCTION=ON -DBUILD_TESTING=OFF -DARTIFACTS_ROOT:PATH="$PWD/build/out-prod"
```

Build production binaries:

```powershell
cmake --build build/meta-prod
```

Run production app:

```powershell
.\build\out-prod\sw\app\bin\hello_world.exe
```

Configure test build tree (Ninja + split output):

```powershell
cmake -S . -B build/meta-tests -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_PRODUCTION=OFF -DBUILD_TESTING=ON -DARTIFACTS_ROOT:PATH="$PWD/build/out-tests"
```

Build unit tests:

```powershell
cmake --build build/meta-tests
```

Run unit tests:

```powershell
ctest --test-dir build/meta-tests --output-on-failure
```

Clean test outputs:

```powershell
cmake --build build/meta-tests --target clean
cmake -E rm -rf build/meta-tests build/out-tests
```
