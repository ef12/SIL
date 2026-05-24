# Implement Application — Software Design Document

---

## 1. Purpose

This document describes the fan controller implement application — a minimal
ISOBUS-style demo that exercises the SIL platform's IO and CAN drivers. The
application reads IO inputs, runs a PI control loop, writes IO outputs, and
exchanges CAN status/command frames.

## 2. Scope

### In Scope

- Application initialization (BSP → HAL → App)
- Fan controller module configuration and main loop
- IO pin mapping and semantics
- CAN protocol (status and command frames)
- Dependencies on SIL library and driver interfaces

### Out of Scope

- Platform/transport implementation (see `SDD_sil_lib.md`)
- Abstract driver interface design (see `SDD_can_driver.md`, `SDD_io_driver.md`)
- GUI operation (see `UM_io_gui.md`)

## 3. Architecture

The implement application follows a three-phase initialization pattern inspired
by embedded BSP → HAL → Application layering:

```
┌──────────────────────────────────────────┐
│          Phase 1: BSP                    │
│  sil_io_config_init()                    │
│  sil_vcan_config_init()                  │
└────────────────┬─────────────────────────┘
                 │
┌────────────────▼─────────────────────────┐
│          Phase 2: HAL                    │
│  sil_io_config_get_driver()  → IoDriver* │
│  sil_vcan_config_get_driver()→ CanDriver*│
└────────────────┬─────────────────────────┘
                 │
┌────────────────▼─────────────────────────┐
│          Phase 3: Application            │
│  fan_controller_init()                   │
│  Main loop: update → send → receive      │
└──────────────────────────────────────────┘
```

This pattern allows the same application code to run on different platforms
(SIL, HIL, target hardware) by substituting only the BSP phase.

## 4. Configuration

### 4.1 Network Ports

| Parameter | Value | Description |
|-----------|-------|-------------|
| `IO_PORT` | 7501 | Local UDP port for IO transport |
| `IO_PEER_PORT` | 7502 | Remote UDP port (GUI side) |
| `CAN_PORT` | 7401 | Local UDP port for CAN transport |
| `CAN_PEER` | 7402 | Remote UDP port (GUI/VT side) |
| `IO_SYNC_RATE_MS` | 10 | IO background sync interval |

### 4.2 IO Pin Mapping

| Type | Pin | Direction | Semantics |
|------|-----|-----------|-----------|
| Digital | 0 | Input | Fan enable (0 = off, 1 = on) |
| Analog | 0 | Input | Fan speed feedback (0–65535) |
| Analog | 1 | Output | Fan drive output (0–65535) |

### 4.3 Fan Controller Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| `setpoint` | 32768 | Default target speed (mid-range) |
| `kp` | 1.0 | PI proportional gain |
| `ki` | 0.05 | PI integral gain |
| `output_min` | 0.0 | Minimum output clamp |
| `output_max` | 65535.0 | Maximum output clamp |

### 4.4 CAN Protocol

| Frame | CAN ID | DLC | Data Layout |
|-------|--------|-----|-------------|
| **Status** (TX) | `0x18FF50E5` | 7 | `[enabled, sp_lo, sp_hi, fb_lo, fb_hi, out_lo, out_hi]` |
| **Command** (RX) | `0x18FF60E5` | 2 | `[sp_lo, sp_hi]` — new setpoint (uint16 LE) |

The app sends a status frame every main loop iteration. When a valid command
frame is received, the setpoint is updated and a status ACK is sent immediately.

### 4.5 Main Loop

| Parameter | Value |
|-----------|-------|
| `LOOP_MS` | 100 |
| `LOOP_DT` | 0.1 s |

Each iteration:

1. `fan_controller_update()` — reads IO, runs PI, writes IO
2. `fan_controller_send_status()` — broadcasts CAN status frame
3. `fan_controller_receive_commands()` — polls CAN for command frames
4. `sleep_ms(100)` — fixed-rate delay

## 5. Dependencies

```
implement
├── fan_controller_lib
│   ├── pi_controller_lib
│   ├── io_driver_lib
│   └── can_driver_lib
│       └── can_frame_lib
├── sil_io_config_lib        (BSP — IO)
│   ├── io_transport_udp_lib
│   └── sil_config_udp_socket_lib
│       └── udp_socket_lib
└── sil_vcan_config_lib      (BSP — CAN)
    ├── can_transport_udp_lib
    └── sil_config_udp_socket_lib
        └── udp_socket_lib
```

## 6. Module Summary

| Module | Purpose |
|--------|---------|
| `fan_controller` | Ties IO + CAN + PI into a coherent fan control behavior |
| `pi_controller` | Generic proportional-integral controller with output clamping |

Both modules live under `sw/modules/` and have dedicated unit tests.

## 7. Verification

### 7.1 Unit Tests

| Test File | Module |
|-----------|--------|
| `sw/modules/fan_controller/test/test_fan_controller.c` | fan_controller |
| `sw/modules/pi_controller/test/test_pi_controller.c` | pi_controller |

(Run via `ruby -S ceedling test:all` from the `test/` directory.)

### 7.2 Manual Integration

1. Build: `cmake --build build/meta`
2. Run: `build/out/implement.exe`
3. Launch GUI: `python tools/io_gui.py`
4. Toggle Fan Enable, adjust Feedback slider, send CAN setpoints
5. Verify status frame updates and fan output response

## 8. Future

- Add ISOBUS VT object-pool upload and operational VT exchange
- Add fault detection (e.g., stall, over-current emulation)
- Add additional implement behaviors beyond the fan demo
