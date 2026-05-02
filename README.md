# SIL

Small C project with CMake, Ninja, Unity, and CMock.

## Highlights

- Production app target: hello_world
- Unit tests integrated with CTest
- Reusable CMake helper for adding new test modules quickly
- Clean separation of build metadata and output artifacts

## Build Layout

| Purpose | Path |
|---|---|
| Production CMake metadata | build/meta |
| Production artifacts | build/out |
| Test CMake metadata | build/meta-tests |
| Test artifacts | build/out-tests |

Main executable:

- build/out/sw/app/bin/hello_world.exe

## Prerequisites

Required:

1. CMake 3.16+
2. GCC toolchain in PATH
3. Ninja in PATH

Optional (unit tests):

1. Ruby in PATH (used by CMock generation)

Install Ninja on Windows if needed:

```powershell
winget install --id Ninja-build.Ninja -e
```

Then restart VS Code or your terminal.

## Quick Start (Production)

Configure:

```powershell
cmake -S . -B build/meta -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_PRODUCTION=ON -DBUILD_TESTING=OFF -DARTIFACTS_ROOT:PATH="$PWD/build/out"
```

Build:

```powershell
cmake --build build/meta
```

Run:

```powershell
.\build\out\sw\app\bin\hello_world.exe
```

Clean:

```powershell
cmake --build build/meta --target clean
cmake -E rm -rf build/out
```

## Quick Start (Unit Tests)

Configure:

```powershell
cmake -S . -B build/meta-tests -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_PRODUCTION=OFF -DBUILD_TESTING=ON -DARTIFACTS_ROOT:PATH="$PWD/build/out-tests"
```

Build:

```powershell
cmake --build build/meta-tests
```

Run tests:

```powershell
ctest --test-dir build/meta-tests --output-on-failure
```

Current status:

- 2/2 tests passing

## VS Code Tasks

Available tasks:

- Configure
- Build
- Clean
- Run Tests
- Clean Tests

Tip: press Ctrl+Shift+B to open the build task list.

## More Testing Details

See test/README.md for module-level testing notes and the helper pattern used to add new tests.

## Troubleshooting

1. If Ninja is not found, reinstall it and restart VS Code.
2. If a build tree has stale configuration, delete its matching metadata and output folders and configure again.
3. Keep production and test builds in separate folders to avoid BUILD_PRODUCTION and BUILD_TESTING conflicts.


