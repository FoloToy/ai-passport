"""Tkinter GUI that simulates Claude Desktop's Hardware Buddy bridge."""

import json
import queue
import time
import tkinter as tk
from tkinter import messagebox, ttk
from typing import Any, Dict

if __package__:
    from .ble_client import BleWorker, DeviceInfo
    from .protocol import (build_heartbeat, build_name, build_owner,
                           build_status_request, build_time_sync, build_unpair,
                           parse_device_message)
    from .scenarios import ScenarioController
else:
    from ble_client import BleWorker, DeviceInfo
    from protocol import (build_heartbeat, build_name, build_owner,
                          build_status_request, build_time_sync, build_unpair,
                          parse_device_message)
    from scenarios import ScenarioController


class BuddyControllerApp(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("Claude Hardware Buddy Controller")
        self.geometry("980x720")
        self.minsize(850, 620)
        self.events: queue.Queue = queue.Queue()
        self.ble = BleWorker(lambda kind, value: self.events.put((kind, value)))
        self.scenario = ScenarioController()
        self.connected = False
        self.auto_running = False
        self.device_by_label: Dict[str, DeviceInfo] = {}
        self._build_ui()
        self.after(50, self._poll_events)
        self.after(10000, self._heartbeat_timer)
        self.protocol("WM_DELETE_WINDOW", self._close)

    def _build_ui(self) -> None:
        connection = ttk.LabelFrame(self, text="BLE connection", padding=8)
        connection.pack(fill="x", padx=8, pady=6)
        self.device_box = ttk.Combobox(connection, state="readonly", width=58)
        self.device_box.pack(side="left", padx=4)
        ttk.Button(connection, text="Scan", command=self.ble.scan).pack(side="left", padx=3)
        ttk.Button(connection, text="Connect", command=self._connect).pack(side="left", padx=3)
        ttk.Button(connection, text="Disconnect", command=self.ble.disconnect).pack(side="left", padx=3)
        self.status_var = tk.StringVar(value="Not connected")
        ttk.Label(connection, textvariable=self.status_var).pack(side="left", padx=12)

        body = ttk.Frame(self)
        body.pack(fill="both", expand=True, padx=8)
        controls = ttk.Frame(body)
        controls.pack(side="left", fill="y", padx=(0, 8))
        log_frame = ttk.LabelFrame(body, text="Protocol log", padding=5)
        log_frame.pack(side="left", fill="both", expand=True)

        identity = ttk.LabelFrame(controls, text="Identity / commands", padding=8)
        identity.pack(fill="x", pady=4)
        self.owner_var = tk.StringVar(value="Tester")
        self.name_var = tk.StringVar(value="FoloBuddy")
        self._entry_row(identity, "Owner", self.owner_var)
        self._entry_row(identity, "Device name", self.name_var)
        ttk.Button(identity, text="Send time + owner", command=self._send_bootstrap).pack(fill="x", pady=2)
        ttk.Button(identity, text="Set device name", command=lambda: self._send(build_name(self.name_var.get()))).pack(fill="x", pady=2)
        ttk.Button(identity, text="Request status", command=lambda: self._send(build_status_request())).pack(fill="x", pady=2)
        ttk.Button(identity, text="Request unpair", command=lambda: self._send(build_unpair())).pack(fill="x", pady=2)

        manual = ttk.LabelFrame(controls, text="Manual scenarios", padding=8)
        manual.pack(fill="x", pady=4)
        for label, action in (("Idle", self.scenario.set_idle),
                              ("Busy", self.scenario.set_busy),
                              ("Completed", self.scenario.set_completed),
                              ("Sleep snapshot", self.scenario.set_sleep)):
            ttk.Button(manual, text=label,
                       command=lambda fn=action: self._scenario(fn)).pack(fill="x", pady=2)

        approval = ttk.LabelFrame(controls, text="Approval prompt", padding=8)
        approval.pack(fill="x", pady=4)
        self.prompt_id_var = tk.StringVar(value="req_test_001")
        self.tool_var = tk.StringVar(value="Bash")
        self.hint_var = tk.StringVar(value="git status")
        self._entry_row(approval, "Request ID", self.prompt_id_var)
        self._entry_row(approval, "Tool", self.tool_var)
        self._entry_row(approval, "Hint", self.hint_var)
        ttk.Button(approval, text="Send approval", command=self._send_approval).pack(fill="x", pady=2)
        ttk.Button(approval, text="Clear prompt", command=lambda: self._scenario(self.scenario.clear_prompt)).pack(fill="x", pady=2)

        automatic = ttk.LabelFrame(controls, text="Automatic cycle", padding=8)
        automatic.pack(fill="x", pady=4)
        self.auto_interval = tk.IntVar(value=8)
        ttk.Spinbox(automatic, from_=3, to=60, textvariable=self.auto_interval, width=6).pack(side="left")
        ttk.Label(automatic, text=" seconds").pack(side="left")
        self.auto_button = ttk.Button(automatic, text="Start", command=self._toggle_auto)
        self.auto_button.pack(side="right")

        self.log = tk.Text(log_frame, wrap="word", state="disabled", font=("Consolas", 10))
        scrollbar = ttk.Scrollbar(log_frame, orient="vertical", command=self.log.yview)
        self.log.configure(yscrollcommand=scrollbar.set)
        self.log.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")
        ttk.Button(self, text="Clear log", command=self._clear_log).pack(anchor="e", padx=10, pady=5)

    @staticmethod
    def _entry_row(parent: ttk.Widget, label: str, variable: tk.StringVar) -> None:
        row = ttk.Frame(parent)
        row.pack(fill="x", pady=1)
        ttk.Label(row, text=label, width=12).pack(side="left")
        ttk.Entry(row, textvariable=variable, width=28).pack(side="left", fill="x", expand=True)

    def _connect(self) -> None:
        info = self.device_by_label.get(self.device_box.get())
        if info is None:
            messagebox.showinfo("Select device", "Scan and select a Claude-* device first.")
            return
        self.ble.connect(info.address)

    def _send(self, payload: bytes) -> None:
        if not self.connected:
            self._append("! Not connected")
            return
        self.ble.send(payload)

    def _send_bootstrap(self) -> None:
        now = int(time.time())
        offset = -int(time.timezone if not time.localtime().tm_isdst else time.altzone)
        self._send(build_time_sync(now, offset))
        self._send(build_owner(self.owner_var.get()))

    def _heartbeat(self) -> None:
        values = self.scenario.heartbeat()
        self._send(build_heartbeat(total=values["total"], running=values["running"],
                                   waiting=values["waiting"], message=values["message"],
                                   entries=values["entries"], tokens=values["tokens"],
                                   tokens_today=values["tokens_today"],
                                   prompt=values.get("prompt")))

    def _scenario(self, action: Any) -> None:
        action()
        self._heartbeat()

    def _send_approval(self) -> None:
        self.scenario.set_approval(self.prompt_id_var.get(), self.tool_var.get(),
                                   self.hint_var.get())
        self._heartbeat()

    def _toggle_auto(self) -> None:
        self.auto_running = not self.auto_running
        self.auto_button.configure(text="Stop" if self.auto_running else "Start")
        if self.auto_running:
            self._auto_step()

    def _auto_step(self) -> None:
        if not self.auto_running:
            return
        scenario = self.scenario.advance_auto()
        self._append("AUTO " + scenario.name)
        self._heartbeat()
        self.after(max(3, self.auto_interval.get()) * 1000, self._auto_step)

    def _heartbeat_timer(self) -> None:
        if self.connected:
            self._heartbeat()
        self.after(10000, self._heartbeat_timer)

    def _poll_events(self) -> None:
        try:
            while True:
                kind, value = self.events.get_nowait()
                if kind == "devices":
                    self.device_by_label = {
                        "%s  [%s]" % (device.name, device.address): device for device in value
                    }
                    self.device_box["values"] = list(self.device_by_label)
                    if self.device_by_label:
                        self.device_box.current(0)
                elif kind == "connected":
                    self.connected = bool(value)
                    if self.connected:
                        self.after(250, self._send_bootstrap)
                        self.after(500, self._heartbeat)
                elif kind == "status":
                    self.status_var.set(str(value))
                elif kind == "tx":
                    self._append("TX " + str(value))
                elif kind == "rx":
                    self._handle_rx(str(value))
                elif kind == "error":
                    self._append("ERROR " + str(value))
        except queue.Empty:
            pass
        self.after(50, self._poll_events)

    def _handle_rx(self, line: str) -> None:
        self._append("RX " + line)
        try:
            message = parse_device_message(line)
            if message.kind == "permission":
                decision = message.payload["decision"].upper()
                self.status_var.set("Permission %s: %s" % (decision, message.payload["id"]))
            elif message.kind == "status":
                self.status_var.set("Status received")
                self._append(json.dumps(message.payload.get("data", {}), indent=2,
                                        ensure_ascii=False))
        except (ValueError, json.JSONDecodeError) as error:
            self._append("PARSE ERROR " + str(error))

    def _append(self, text: str) -> None:
        self.log.configure(state="normal")
        self.log.insert("end", time.strftime("%H:%M:%S ") + text + "\n")
        self.log.see("end")
        self.log.configure(state="disabled")

    def _clear_log(self) -> None:
        self.log.configure(state="normal")
        self.log.delete("1.0", "end")
        self.log.configure(state="disabled")

    def _close(self) -> None:
        self.auto_running = False
        self.ble.stop()
        self.destroy()


def main() -> None:
    BuddyControllerApp().mainloop()


if __name__ == "__main__":
    main()
