# SIL

Small C project with CMake/Ninja for production builds and Ceedling (Unity + CMock) for unit tests.

## Highlights

- Production app target: hello_world
- Core transport module: udp_socket
- Unit tests run through Ceedling from the test folder
- Every test file named test_*.c is auto-discovered by Ceedling
- Clean separation of production and test build artifacts

## Build Layout

| Purpose | Path |
|---|---|
| Production CMake metadata | build/meta |
| Production artifacts | build/out |
| Ceedling test build artifacts | ceedling |

Main executable:

- build/out/sw/app/bin/hello_world.exe

## Prerequisites

Required:

1. CMake 3.16+
2. GCC toolchain in PATH
3. Ninja in PATH

Optional (unit tests):

1. Ruby in PATH
2. Ceedling gem installed (`gem install ceedling`)

Install Ninja on Windows if needed:

```powershell
winget install --id Ninja-build.Ninja -e
```

Then restart VS Code or your terminal.

## Quick Start (Production)

Configure:

```powershell
cmake -S . -B build/meta -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_PRODUCTION=ON -DARTIFACTS_ROOT:PATH="$PWD/build/out"
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

Run all unit tests:

```powershell
Set-Location test
ruby -S ceedling test:all
```

Current status:

- 6/6 tests passing

Clean unit test artifacts:

```powershell
Set-Location test
ruby -S ceedling clobber
cmake -E rm -rf ../ceedling
```

## VS Code Tasks

Available tasks:

- Configure
- Build
- Clean
- Run Tests
- Clean Tests

Tip: press Ctrl+Shift+B to open the build task list.

## More Testing Details

See test/README.md for Ceedling configuration, test discovery rules, and module test guidance.

## Troubleshooting

1. If Ninja is not found, reinstall it and restart VS Code.
2. If a build tree has stale configuration, delete its matching metadata and output folders and configure again.
3. If Ceedling is not found, run `gem install ceedling` and reopen the terminal.


