# IO GUI — User Manual

## Overview

`io_gui.py` is a Python/tkinter GUI that acts as the simulated plant for the
SIL fan controller application (`implement`). It communicates over UDP using
the IO transport protocol (pins) and the CAN transport protocol (setpoint commands).

## Requirements

- Python 3.10+ (tkinter is included in standard Python on Windows)
- No external packages required

## Quick Start

Start the GUI (defaults match the `implement` application):

```powershell
python tools\io_gui.py
```

Start the application:

```powershell
.\build\out\sw\apps\implement\bin\implement.exe
```

The order does not matter — the GUI survives starting before the application.

## Command-Line Options

| Flag | Default | Description |
|------|---------|-------------|
| `--local-port` | 7502 | IO UDP port the GUI binds |
| `--remote-ip` | 127.0.0.1 | Application IP address |
| `--remote-port` | 7501 | Application IO UDP port |
| `--digital-pins` | 1 | Number of digital IO pins |
| `--analog-pins` | 2 | Number of analog IO pins |
| `--can-local-port` | 7402 | CAN UDP port the GUI binds |
| `--can-remote-port` | 7401 | Application CAN UDP port |

## GUI Layout

### Controls → App (left column, top)

| Control | Pin | Description |
|---------|-----|-------------|
| Fan Enable | Digital 0 | Checkbox — enables the fan controller in the app |
| Feedback (sensor) | Analog 0 | Slider (0–65535) — simulates fan speed sensor input |

The feedback slider responds to mouse clicks and keyboard arrows (±1 per press).

### App → Monitor (left column, bottom)

| Monitor | Pin | Description |
|---------|-----|-------------|
| Fan Enable | Digital 0 | Shows ON/OFF as received from the app |
| Output (PWM) | Analog 1 | Progress bar — fan controller PWM output |

### CAN Commands → App (right column, top)

| Element | Description |
|---------|-------------|
| Setpoint entry | Type a value 0–65535, press Enter to send immediately |
| Connection status | Green "Connected" / Red "Disconnected" |
| App setpoint | Displays the setpoint the application is actually using |

The setpoint is sent automatically every 1 second. Connection status is based on
whether the application responds with a CAN status message within 2 seconds.

### Fan Animation (right column, bottom)

A rotating 4-blade fan visualization. Rotation speed is proportional to the
feedback sensor value (analog 0).

## Network Architecture

```
┌─────────────┐         UDP IO (port 7501↔7502)         ┌──────────────┐
│   IO GUI    │◄────────────────────────────────────────►│  implement   │
│  (Python)   │         UDP CAN (port 7401↔7402)        │   (C app)    │
│             │◄────────────────────────────────────────►│              │
└─────────────┘                                          └──────────────┘
```

### IO Wire Protocol

6-byte header (`IO`, version, digital count, analog count, reserved) followed by
1 byte per digital pin and 2 bytes (LE uint16) per analog pin.

### CAN Wire Protocol

13 bytes: CAN ID (4 bytes, big-endian) + DLC (1 byte) + data (8 bytes).

| CAN ID | Direction | DLC | Content |
|--------|-----------|-----|---------|
| 0x18FF60E5 | GUI → App | 2 | Setpoint (uint16 LE) |
| 0x18FF50E5 | App → GUI | 7 | enabled(1), setpoint(2), feedback(2), output(2) |

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| GUI stays "Disconnected" | Application not running or firewall blocking | Start the app; allow through Windows Firewall |
| Fan not spinning | Fan Enable checkbox not checked, or feedback slider at 0 | Check the enable box and move the feedback slider |
| IO monitor shows all zeros | Application not running | Start `implement.exe` |
