# SIL

This project uses CMake with the Ninja generator.

Build layout:
- CMake metadata: build/meta
- Final artifacts: build/out

Main production executable:
- build/out/sw/app/bin/hello_world.exe

Prerequisites:
1. CMake 3.16 or newer
2. GCC toolchain available in PATH
3. Ninja available in PATH

Optional for unit tests:
1. Ruby available in PATH (used by CMock generation)

Clone and open:
1. git clone <your-repo-url>
2. cd SIL

Install Ninja on Windows (if missing):
1. winget install --id Ninja-build.Ninja -e
2. Restart terminal after install

Build (production):
1. cmake -S . -B build/meta -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_PRODUCTION=ON -DBUILD_TESTING=OFF -DARTIFACTS_ROOT:PATH=%CD%/build/out
2. cmake --build build/meta

Run:
1. .\build\out\sw\app\bin\hello_world.exe

Clean:
1. cmake --build build/meta --target clean
2. cmake -E rm -rf build/out

Build unit tests:
1. cmake -S . -B build/meta -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_PRODUCTION=OFF -DBUILD_TESTING=ON -DARTIFACTS_ROOT:PATH=%CD%/build/out
2. cmake --build build/meta

Run unit tests:
1. ctest --test-dir build/meta --output-on-failure

VS Code tasks:
1. Build
2. Clean

Troubleshooting:
1. If Ninja is not found, install it and restart terminal/VS Code.
2. If old build folders cause path conflicts, remove build/meta and build/out, then reconfigure.


