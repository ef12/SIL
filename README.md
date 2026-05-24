# SIL

Software-in-the-Loop platform for agricultural implement development.
Uses CMake/Ninja for production builds and Ceedling (Unity + CMock) for unit tests.

## Highlights

- Production app target: `implement` (fan controller demo)
- IO GUI tool: `tools/io_gui.py` (Python/tkinter plant simulator)
- Core libraries: `udp_socket`, `can_frame`, `can_driver`, `can_transport_udp`, `can_emulator`, `io_driver`, `io_transport_udp`
- SIL configuration: `sil_io_config` (IO + sync thread), `sil_vcan_config` (virtual CAN)
- Application modules: `pi_controller`, `fan_controller`
- Unit tests run through Ceedling from the `test/` folder

## Directory Structure

```
sw/
  apps/
    implement/          Fan controller application
    main.c              Standalone hello-world
  drivers/
    can_driver/         Abstract CAN driver interface
      can_frame/        CAN frame data model
    io/                 Abstract IO driver interface
  modules/
    fan_controller/     Fan speed controller with PI loop
    pi_controller/      Generic PI controller
sil_lib/
  udp_socket/           Raw UDP socket + SIL socket helper
  io/
    io_transport_udp/   IO pin serialization over UDP
    sil_io_config       IO BSP with sync thread
  vcan/
    can_transport_udp/  CAN frame serialization over UDP
    can_emulator/       Virtual CAN bus emulator
    sil_vcan_config     CAN BSP
tools/
  io_gui.py             Python GUI (plant simulator)
test/
  project.yml           Ceedling configuration
```

## Build Layout

| Purpose | Path |
|---------|------|
| CMake metadata | `build/meta/` |
| Build artifacts | `build/out/` |
| Ceedling test artifacts | `ceedling/` |

Application executable: `build/out/sw/apps/implement/bin/implement.exe`

## Prerequisites

Required:

1. CMake 3.16+
2. GCC toolchain in PATH
3. Ninja in PATH

Optional (unit tests):

1. Ruby in PATH
2. Ceedling gem (`gem install ceedling`)

Optional (GUI):

1. Python 3.10+ (tkinter included on Windows)

## Quick Start (Production)

Configure:

```powershell
cmake -S . -B build/meta -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_PRODUCTION=ON -DARTIFACTS_ROOT:PATH="$PWD/build/out"
```

Build:

```powershell
cmake --build build/meta
```

Run the application and GUI:

```powershell
.\build\out\sw\apps\implement\bin\implement.exe
python tools\io_gui.py
```

## Quick Start (Unit Tests)

```powershell
Set-Location test
ruby -S ceedling test:all
```

## Documentation

| Document | Location | Type |
|----------|----------|------|
| SIL Library & System Architecture | [sil_lib/SDD_sil_lib.md](sil_lib/SDD_sil_lib.md) | SDD |
| Implement Application | [sw/apps/implement/SDD_implement.md](sw/apps/implement/SDD_implement.md) | SDD |
| SIL IO | [sil_lib/io/SDD_sil_io.md](sil_lib/io/SDD_sil_io.md) | SDD |
| SIL vCAN | [sil_lib/vcan/SDD_sil_vcan.md](sil_lib/vcan/SDD_sil_vcan.md) | SDD |
| SIL UDP Socket | [sil_lib/udp_socket/SDD_sil_udp_socket.md](sil_lib/udp_socket/SDD_sil_udp_socket.md) | SDD |
| CAN Driver | [sw/drivers/can_driver/SDD_can_driver.md](sw/drivers/can_driver/SDD_can_driver.md) | SDD |
| CAN Frame | [sw/drivers/can_driver/can_frame/SDD_can_frame.md](sw/drivers/can_driver/can_frame/SDD_can_frame.md) | SDD |
| IO Driver | [sw/drivers/io/SDD_io_driver.md](sw/drivers/io/SDD_io_driver.md) | SDD |
| Unit Testing Guide | [test/UM_testing.md](test/UM_testing.md) | UM |
| IO GUI Manual | [tools/UM_io_gui.md](tools/UM_io_gui.md) | UM |

## VS Code Tasks

- Configure — CMake configure
- Build — CMake build
- Clean — Remove build artifacts
- Run Tests — Ceedling test:all
- Clean Tests — Ceedling clobber
- Run Tests
- Clean Tests

Tip: press Ctrl+Shift+B to open the build task list.

## More Testing Details

See test/README.md for Ceedling configuration, test discovery rules, and module test guidance.

## Troubleshooting

1. If Ninja is not found, reinstall it and restart VS Code.
2. If a build tree has stale configuration, delete its matching metadata and output folders and configure again.
3. If Ceedling is not found, run `gem install ceedling` and reopen the terminal.


