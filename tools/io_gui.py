"""
SIL IO Monitor & Control GUI.

Acts as the simulated plant for the fan controller application.
Communicates via the io_transport_udp wire protocol for IO pins
and via the can_transport_udp wire protocol for CAN messages.

Default pin layout (matching can_app1):
  Digital 0: Fan Enable   (control → app reads)
  Analog  0: Fan Feedback (control → app reads)
  Analog  1: Fan Output   (monitor ← app writes)

CAN messages (matching fan_controller):
  Command 0x18FF60E5: setpoint (GUI → app)
  Status  0x18FF50E5: enabled, setpoint, feedback, output (app → GUI)
"""

import argparse
import math
import socket
import struct
import threading
import time
import tkinter as tk
from tkinter import ttk

# ── Wire protocol ────────────────────────────────────────────────────

PROTO_MAGIC = b"IO"
PROTO_VERSION = 1
HEADER_SIZE = 6  # 'I' 'O' ver dig_count ana_count reserved


def encode_payload(digital_values: list[bool], analog_values: list[int]) -> bytes:
    header = struct.pack(
        "2sBBBB",
        PROTO_MAGIC,
        PROTO_VERSION,
        len(digital_values),
        len(analog_values),
        0,
    )
    dig = bytes(1 if v else 0 for v in digital_values)
    ana = b"".join(struct.pack("<H", v & 0xFFFF) for v in analog_values)
    return header + dig + ana


def decode_payload(
    data: bytes, digital_count: int, analog_count: int
) -> tuple[list[bool], list[int]] | None:
    expected = HEADER_SIZE + digital_count + analog_count * 2
    if len(data) != expected:
        return None
    if data[0:2] != PROTO_MAGIC or data[2] != PROTO_VERSION:
        return None
    if data[3] != digital_count or data[4] != analog_count:
        return None

    offset = HEADER_SIZE
    digitals = [data[offset + i] != 0 for i in range(digital_count)]
    offset += digital_count
    analogs = [
        struct.unpack_from("<H", data, offset + i * 2)[0]
        for i in range(analog_count)
    ]
    return digitals, analogs


# ── CAN wire protocol ────────────────────────────────────────────────

CAN_WIRE_SIZE = 13  # 4 (id BE) + 1 (dlc) + 8 (data)
COMMAND_CAN_ID = 0x18FF60E5
STATUS_CAN_ID = 0x18FF50E5
CAN_TIMEOUT_S = 2.0  # consider disconnected after this


def can_encode(can_id: int, dlc: int, data: bytes) -> bytes:
    return struct.pack(">IB", can_id, dlc) + data.ljust(8, b"\x00")[:8]


# ── UDP CAN bridge ───────────────────────────────────────────────────


class SilCanBridge:
    """Sends CAN commands to the SIL app, tracks connection via responses."""

    def __init__(self, local_port: int, remote_ip: str, remote_port: int):
        self.remote_ip = remote_ip
        self.remote_port = remote_port
        self._last_rx_time = 0.0
        self._rx_setpoint = 0
        self._lock = threading.Lock()

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("0.0.0.0", local_port))
        self.sock.settimeout(0.1)

        self._running = True
        self._thread = threading.Thread(target=self._rx_loop, daemon=True)
        self._thread.start()

    def send_setpoint(self, setpoint: int) -> None:
        data = struct.pack("<H", setpoint & 0xFFFF)
        payload = can_encode(COMMAND_CAN_ID, 2, data)
        self.sock.sendto(payload, (self.remote_ip, self.remote_port))

    def get_rx_setpoint(self) -> int:
        with self._lock:
            return self._rx_setpoint

    def is_connected(self) -> bool:
        with self._lock:
            return (time.monotonic() - self._last_rx_time) < CAN_TIMEOUT_S

    def close(self) -> None:
        self._running = False
        self._thread.join(timeout=1.0)
        self.sock.close()

    def _rx_loop(self) -> None:
        while self._running:
            try:
                data, _ = self.sock.recvfrom(256)
            except OSError:
                continue
            if len(data) < CAN_WIRE_SIZE:
                continue
            can_id = struct.unpack_from(">I", data, 0)[0]
            if can_id == STATUS_CAN_ID:
                dlc = data[4]
                with self._lock:
                    self._last_rx_time = time.monotonic()
                    if dlc >= 3:
                        self._rx_setpoint = struct.unpack_from("<H", data, 6)[0]


# ── UDP IO bridge ────────────────────────────────────────────────────


class SilIoBridge:
    """Sends control values to the SIL app, receives monitored outputs."""

    def __init__(
        self,
        local_port: int,
        remote_ip: str,
        remote_port: int,
        digital_count: int,
        analog_count: int,
    ):
        self.remote_ip = remote_ip
        self.remote_port = remote_port
        self.digital_count = digital_count
        self.analog_count = analog_count

        # Control values (GUI → app)
        self.tx_digital = [False] * digital_count
        self.tx_analog = [0] * analog_count

        # Monitored values (app → GUI)
        self.rx_digital = [False] * digital_count
        self.rx_analog = [0] * analog_count
        self.rx_lock = threading.Lock()

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("0.0.0.0", local_port))
        self.sock.settimeout(0.1)

        self._running = True
        self._thread = threading.Thread(target=self._rx_loop, daemon=True)
        self._thread.start()

    def send(self) -> None:
        payload = encode_payload(self.tx_digital, self.tx_analog)
        self.sock.sendto(payload, (self.remote_ip, self.remote_port))

    def get_rx(self) -> tuple[list[bool], list[int]]:
        with self.rx_lock:
            return list(self.rx_digital), list(self.rx_analog)

    def close(self) -> None:
        self._running = False
        self._thread.join(timeout=1.0)
        self.sock.close()

    def _rx_loop(self) -> None:
        while self._running:
            try:
                data, _ = self.sock.recvfrom(1024)
            except OSError:
                continue
            result = decode_payload(data, self.digital_count, self.analog_count)
            if result is not None:
                with self.rx_lock:
                    self.rx_digital, self.rx_analog = result


# ── GUI ──────────────────────────────────────────────────────────────

DIGITAL_LABELS = ["Fan Enable"]
ANALOG_LABELS = ["Feedback (sensor)", "Output (PWM)"]
ANALOG_CONTROL_PINS = {0}  # pins the user can control
ANALOG_MONITOR_PINS = {1}  # pins the GUI only displays

FAN_CANVAS_SIZE = 160
FAN_RADIUS = 60
FAN_BLADES = 4
FAN_MAX_SPEED = 36.0  # max degrees per poll tick at full output


class IoGui:
    def __init__(self, bridge: SilIoBridge, can_bridge: SilCanBridge | None = None) -> None:
        self.bridge = bridge
        self.can_bridge = can_bridge

        self.root = tk.Tk()
        self.root.title("SIL IO Monitor")
        self.root.resizable(False, False)

        self._build_controls()
        self._build_monitors()
        self._build_fan_animation()
        if self.can_bridge is not None:
            self._build_can_controls()

        self._poll()
        if self.can_bridge is not None:
            self._can_heartbeat()

    # ── layout ──

    def _build_controls(self) -> None:
        frame = ttk.LabelFrame(self.root, text="Controls → App", padding=10)
        frame.grid(row=0, column=0, padx=10, pady=5, sticky="nsew")

        self.digital_vars: list[tk.BooleanVar] = []
        for i in range(self.bridge.digital_count):
            label = DIGITAL_LABELS[i] if i < len(DIGITAL_LABELS) else f"Digital {i}"
            var = tk.BooleanVar(value=False)
            cb = ttk.Checkbutton(
                frame, text=label, variable=var, command=self._on_control_change
            )
            cb.grid(row=i, column=0, sticky="w", pady=2)
            self.digital_vars.append(var)

        self.analog_sliders: dict[int, tk.Scale] = {}
        row = self.bridge.digital_count
        for pin in sorted(ANALOG_CONTROL_PINS):
            if pin >= self.bridge.analog_count:
                continue
            label = ANALOG_LABELS[pin] if pin < len(ANALOG_LABELS) else f"Analog {pin}"
            ttk.Label(frame, text=label).grid(row=row, column=0, sticky="w")
            slider = tk.Scale(
                frame,
                from_=0,
                to=65535,
                orient=tk.HORIZONTAL,
                length=300,
                resolution=1,
                bigincrement=1,
                command=lambda _v, _p=pin: self._on_control_change(),
            )
            slider.grid(row=row + 1, column=0, sticky="ew", pady=(0, 5))
            self.analog_sliders[pin] = slider
            row += 2

    def _build_monitors(self) -> None:
        frame = ttk.LabelFrame(self.root, text="App → Monitor", padding=10)
        frame.grid(row=1, column=0, padx=10, pady=5, sticky="nsew")

        self.monitor_digital_labels: list[ttk.Label] = []
        for i in range(self.bridge.digital_count):
            label_text = DIGITAL_LABELS[i] if i < len(DIGITAL_LABELS) else f"Digital {i}"
            ttk.Label(frame, text=label_text).grid(row=i, column=0, sticky="w")
            val_label = ttk.Label(frame, text="—", width=12)
            val_label.grid(row=i, column=1, sticky="w", padx=(10, 0))
            self.monitor_digital_labels.append(val_label)

        self.monitor_analog_bars: dict[int, ttk.Progressbar] = {}
        self.monitor_analog_labels: dict[int, ttk.Label] = {}
        row = self.bridge.digital_count
        for pin in sorted(ANALOG_MONITOR_PINS):
            if pin >= self.bridge.analog_count:
                continue
            label_text = ANALOG_LABELS[pin] if pin < len(ANALOG_LABELS) else f"Analog {pin}"
            ttk.Label(frame, text=label_text).grid(row=row, column=0, sticky="w")
            bar = ttk.Progressbar(
                frame, orient=tk.HORIZONTAL, length=300, maximum=65535
            )
            bar.grid(row=row, column=1, sticky="ew", padx=(10, 0))
            val_label = ttk.Label(frame, text="0", width=8)
            val_label.grid(row=row, column=2, padx=(5, 0))
            self.monitor_analog_bars[pin] = bar
            self.monitor_analog_labels[pin] = val_label
            row += 1

    def _build_can_controls(self) -> None:
        frame = ttk.LabelFrame(self.root, text="CAN Commands → App", padding=10)
        frame.grid(row=0, column=1, padx=10, pady=5, sticky="nsew")

        ttk.Label(frame, text="Setpoint (0–65535)").grid(row=0, column=0, sticky="w")
        self.setpoint_entry = ttk.Entry(frame, width=10)
        self.setpoint_entry.grid(row=0, column=1, padx=(10, 5))
        self.setpoint_entry.insert(0, "32768")
        self.setpoint_entry.bind("<Return>", lambda _e: self._on_setpoint_send())


        self.can_status_label = ttk.Label(frame, text="Disconnected", foreground="red")
        self.can_status_label.grid(row=1, column=0, columnspan=3, sticky="w", pady=(5, 0))

        ttk.Label(frame, text="App setpoint:").grid(row=2, column=0, sticky="w", pady=(5, 0))
        self.app_setpoint_label = ttk.Label(frame, text="—", width=10)
        self.app_setpoint_label.grid(row=2, column=1, sticky="w", padx=(10, 0), pady=(5, 0))
    def _build_fan_animation(self) -> None:
        frame = ttk.LabelFrame(self.root, text="Fan", padding=5)
        frame.grid(row=1, column=1, padx=10, pady=5, sticky="nsew")

        size = FAN_CANVAS_SIZE
        self.fan_canvas = tk.Canvas(frame, width=size, height=size, bg="white")
        self.fan_canvas.pack()
        self.fan_angle = 0.0
        self._draw_fan(0.0)

    def _draw_fan(self, angle_deg: float) -> None:
        c = self.fan_canvas
        c.delete("all")
        cx = cy = FAN_CANVAS_SIZE / 2
        r = FAN_RADIUS
        blade_w = 18

        for i in range(FAN_BLADES):
            a = math.radians(angle_deg + i * (360.0 / FAN_BLADES))
            tip_x = cx + r * math.cos(a)
            tip_y = cy - r * math.sin(a)
            perp = a + math.pi / 2
            pts = [
                cx + 5 * math.cos(a + 0.4), cy - 5 * math.sin(a + 0.4),
                tip_x + blade_w * math.cos(perp), tip_y - blade_w * math.sin(perp),
                tip_x - blade_w * math.cos(perp), tip_y + blade_w * math.sin(perp),
                cx + 5 * math.cos(a - 0.4), cy - 5 * math.sin(a - 0.4),
            ]
            c.create_polygon(pts, fill="#4a90d9", outline="#2c5f8a", width=1)

        hub_r = 10
        c.create_oval(cx - hub_r, cy - hub_r, cx + hub_r, cy + hub_r,
                      fill="#333333", outline="#111111", width=2)

    # ── callbacks ──

    def _on_control_change(self) -> None:
        for i, var in enumerate(self.digital_vars):
            self.bridge.tx_digital[i] = var.get()
        for pin, slider in self.analog_sliders.items():
            self.bridge.tx_analog[pin] = slider.get()
        self.bridge.send()

    def _on_setpoint_send(self) -> None:
        if self.can_bridge is None:
            return
        try:
            val = int(self.setpoint_entry.get())
            val = max(0, min(65535, val))
            self.can_bridge.send_setpoint(val)
        except ValueError:
            pass

    def _can_heartbeat(self) -> None:
        """Send setpoint every second and update connection status."""
        if self.can_bridge is not None:
            self._on_setpoint_send()
            connected = self.can_bridge.is_connected()
            self.can_status_label.config(
                text="Connected" if connected else "Disconnected",
                foreground="green" if connected else "red",
            )
            self.app_setpoint_label.config(
                text=str(self.can_bridge.get_rx_setpoint()) if connected else "—"
            )
        self.root.after(1000, self._can_heartbeat)

    def _poll(self) -> None:
        rx_dig, rx_ana = self.bridge.get_rx()

        # Preserve app-owned (monitor-only) pins in our TX buffer so we
        # don't overwrite them with 0 on the next send.
        for pin in ANALOG_MONITOR_PINS:
            if pin < len(rx_ana):
                self.bridge.tx_analog[pin] = rx_ana[pin]

        for i, label in enumerate(self.monitor_digital_labels):
            if i < len(rx_dig):
                label.config(text="ON" if rx_dig[i] else "OFF")

        for pin, bar in self.monitor_analog_bars.items():
            if pin < len(rx_ana):
                bar["value"] = rx_ana[pin]
                self.monitor_analog_labels[pin].config(text=str(rx_ana[pin]))

        # Update fan animation — speed proportional to feedback sensor (analog 0)
        feedback_val = self.bridge.tx_analog[0] if len(self.bridge.tx_analog) > 0 else 0
        speed = FAN_MAX_SPEED * (feedback_val / 65535.0)
        self.fan_angle = (self.fan_angle + speed) % 360.0
        self._draw_fan(self.fan_angle)

        # Keep sending controls so the app always has fresh data
        self._on_control_change()

        self.root.after(100, self._poll)

    def run(self) -> None:
        try:
            self.root.mainloop()
        finally:
            self.bridge.close()
            if self.can_bridge is not None:
                self.can_bridge.close()


# ── main ─────────────────────────────────────────────────────────────


def main() -> None:
    parser = argparse.ArgumentParser(description="SIL IO Monitor & Control")
    parser.add_argument(
        "--local-port", type=int, default=7502, help="UDP port to listen on (default: 7502)"
    )
    parser.add_argument(
        "--remote-ip", default="127.0.0.1", help="App IP address (default: 127.0.0.1)"
    )
    parser.add_argument(
        "--remote-port", type=int, default=7501, help="App UDP port (default: 7501)"
    )
    parser.add_argument(
        "--digital-pins", type=int, default=1, help="Number of digital pins (default: 1)"
    )
    parser.add_argument(
        "--analog-pins", type=int, default=2, help="Number of analog pins (default: 2)"
    )
    parser.add_argument(
        "--can-local-port", type=int, default=7402, help="CAN UDP port to listen on (default: 7402)"
    )
    parser.add_argument(
        "--can-remote-port", type=int, default=7401, help="App CAN UDP port (default: 7401)"
    )
    args = parser.parse_args()

    bridge = SilIoBridge(
        local_port=args.local_port,
        remote_ip=args.remote_ip,
        remote_port=args.remote_port,
        digital_count=args.digital_pins,
        analog_count=args.analog_pins,
    )

    can_bridge = SilCanBridge(
        local_port=args.can_local_port,
        remote_ip=args.remote_ip,
        remote_port=args.can_remote_port,
    )

    gui = IoGui(bridge, can_bridge)
    gui.run()


if __name__ == "__main__":
    main()
