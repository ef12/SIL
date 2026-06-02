# SIL Consumer Example

Minimal standalone project that links against an **installed** SilLib package.
It validates that `find_package(SilLib)` works, the public headers compile, and
the init/deinit API is callable at runtime.

## Prerequisites

SilLib must be installed first (from the repository root):

```powershell
cmake -S . -B build/meta -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_PRODUCTION=ON
cmake --build build/meta
cmake --install build/meta --prefix build/install
```

## Build

```powershell
cmake -S examples/sil_consumer -B build/sil_consumer -G Ninja `
      -DCMAKE_PREFIX_PATH="$PWD/build/install"
cmake --build build/sil_consumer
```

## Run

```powershell
.\build\sil_consumer\sil_consumer.exe
```

Expected output:

```
SilLib version: 1.0.0
vcan: initialized, driver=<addr>
io:   initialized, driver=<addr>
cleanup complete
```
