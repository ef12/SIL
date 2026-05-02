# Unit Testing Guide

This project uses:

- Unity for test assertions and test runner flow
- CMock for generated mocks from C headers
- CMake + CTest for configure, build, and execution

## Current Test Targets

| Target | Source File | CTest Name |
|---|---|---|
| greeter_tests | sw/greeter/test/test_greeter.c | greeter_unit_tests |
| hello_output_tests | sw/hello_output/test/test_hello_output.c | hello_output_unit_tests |

Latest validation:

- 2/2 tests passed with ctest

## Test Build Layout

| Purpose | Path |
|---|---|
| Test CMake metadata | build/meta-tests |
| Test artifacts | build/out-tests |
| Generated mocks | build/meta-tests/test/generated/mocks |

## Commands

Run from repository root.

Configure tests:

```powershell
cmake -S . -B build/meta-tests -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_PRODUCTION=OFF -DBUILD_TESTING=ON -DARTIFACTS_ROOT:PATH="$PWD/build/out-tests"
```

Build tests:

```powershell
cmake --build build/meta-tests
```

Run tests:

```powershell
ctest --test-dir build/meta-tests --output-on-failure
```

Clean tests:

```powershell
cmake --build build/meta-tests --target clean
cmake -E rm -rf build/meta-tests build/out-tests
```

## Add a New Unit Test Module

1. Create a new test file in sw/<module>/test.
2. Register it in CMakeLists.txt using add_unity_cmock_test(...).
3. Fill these arguments in the helper call:

- TARGET: unique executable target name
- TEST_FILE: new test source file
- TEST_NAME: ctest-visible test name
- OUTPUT_PATH: relative module output path under sw/
- SOURCES: production source files under test
- INCLUDE_DIRS: include directories needed by the test
- MOCK_HEADERS: headers to mock with CMock (optional)

4. Reconfigure, build, and run ctest.

## Notes

- The helper add_unity_cmock_test(...) centralizes executable creation, include wiring, output layout, and add_test registration.
- Generated mocks are managed through test/generate_mock.rb and test/cmock.yml.in.
