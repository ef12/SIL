"""
SIL IO Monitor & Control GUI.

Acts as the simulated plant for the fan controller application.
Communicates via the io_transport_udp wire protocol.

Default pin layout (matching can_app1):
  Digital 0: Fan Enable   (control → app reads)
  Analog  0: Fan Feedback (control → app reads)
  Analog  1: Fan Output   (monitor ← app writes)
"""

import argparse
import socket
import struct
import threading
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
            except socket.timeout:
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


class IoGui:
    def __init__(self, bridge: SilIoBridge) -> None:
        self.bridge = bridge

        self.root = tk.Tk()
        self.root.title("SIL IO Monitor")
        self.root.resizable(False, False)

        self._build_controls()
        self._build_monitors()

        self._poll()

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

    # ── callbacks ──

    def _on_control_change(self) -> None:
        for i, var in enumerate(self.digital_vars):
            self.bridge.tx_digital[i] = var.get()
        for pin, slider in self.analog_sliders.items():
            self.bridge.tx_analog[pin] = slider.get()
        self.bridge.send()

    def _poll(self) -> None:
        rx_dig, rx_ana = self.bridge.get_rx()

        for i, label in enumerate(self.monitor_digital_labels):
            if i < len(rx_dig):
                label.config(text="ON" if rx_dig[i] else "OFF")

        for pin, bar in self.monitor_analog_bars.items():
            if pin < len(rx_ana):
                bar["value"] = rx_ana[pin]
                self.monitor_analog_labels[pin].config(text=str(rx_ana[pin]))

        # Keep sending controls so the app always has fresh data
        self._on_control_change()

        self.root.after(100, self._poll)

    def run(self) -> None:
        try:
            self.root.mainloop()
        finally:
            self.bridge.close()


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
    args = parser.parse_args()

    bridge = SilIoBridge(
        local_port=args.local_port,
        remote_ip=args.remote_ip,
        remote_port=args.remote_port,
        digital_count=args.digital_pins,
        analog_count=args.analog_pins,
    )

    gui = IoGui(bridge)
    gui.run()


if __name__ == "__main__":
    main()
