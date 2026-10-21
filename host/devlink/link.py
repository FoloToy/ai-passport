"""串口传输层：只管把字节送出去、把消息收上来，不懂任何业务。

设计取舍
--------
- 读用**独立线程**持续排空 OS 缓冲：asyncio 直接 read 串口会漏字节，
  而且 CDC 的缓冲很小，不持续读会反压设备。
- 写加锁：心跳、语音、播报可能并发发。
- **必须循环写**：`Serial.write` 底层是 WriteFile，理论上写完，但 Windows 上
  大块写会被拆；这里按剩余长度循环，确保一条消息完整出去（固件侧的
  usb_serial_jtag_write_bytes 也有同样的坑，那边已循环）。
"""
from __future__ import annotations

import asyncio
import queue
import threading
import time

from . import proto


def find_device_port() -> str:
    """按 VID:PID 找 ESP32-C3 的 USB-Serial/JTAG。找不到返回空串。"""
    try:
        import serial.tools.list_ports as lp
    except Exception:
        return ""
    ports = list(lp.comports())
    for p in ports:
        if "303A:1001" in (p.hwid or "").upper():
            return p.device
    for p in ports:
        if "303A" in (p.hwid or "").upper():
            return p.device
    return ""


class Link:
    def __init__(self, port: str = "", *, on_line=None):
        self.port = port
        self.ser = None
        self.connected = False
        self.last_rx = 0.0
        self.fw = ""
        self._on_line = on_line
        self._lines: queue.Queue[str] = queue.Queue()
        self._reader: threading.Thread | None = None
        self._stop = threading.Event()
        self._wlock = threading.Lock()

    # ------------------------------------------------ 生命周期

    def open(self, retry: bool = True) -> bool:
        if self.ser and self.connected:
            return True
        import serial

        port = self.port or find_device_port()
        if not port:
            return False
        try:
            self.ser = serial.Serial(port, 115200, timeout=0)
        except Exception:
            self.ser = None
            self.connected = False
            if retry:
                self.port = port
            return False
        self.port = port
        self.connected = True
        self.last_rx = time.time()
        self._stop.clear()
        self._lines = queue.Queue()
        self._reader = threading.Thread(target=self._read_loop, name="wb-serial",
                                        daemon=True)
        self._reader.start()
        return True

    def close(self) -> None:
        self.connected = False
        self._stop.set()
        if self._reader:
            self._reader.join(timeout=1.5)
            self._reader = None
        if self.ser:
            try:
                self.ser.close()
            except Exception:
                pass
            self.ser = None

    @property
    def alive(self) -> bool:
        """串口在、且最近 60s 有过任何上行（否则可能拔了/崩了）。"""
        return bool(self.ser and self.connected and (time.time() - self.last_rx) < 60)

    # ------------------------------------------------ 收发

    def _read_loop(self) -> None:
        buf = b""
        while not self._stop.is_set():
            try:
                chunk = self.ser.read(8192) if self.ser else b""
            except Exception:
                break
            if not chunk:
                time.sleep(0.005)
                continue
            self.last_rx = time.time()
            buf += chunk
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                msg = proto.parse(raw)
                if msg:
                    self._lines.put(msg)
        self.connected = False

    def send(self, kind: str, **payload) -> bool:
        if not (self.ser and self.connected):
            return False
        data = proto.encode(kind, **payload)
        try:
            with self._wlock:
                self.ser.write(data)
            return True
        except Exception:
            self.connected = False
            return False

    def send_raw(self, data: bytes) -> bool:
        if not (self.ser and self.connected):
            return False
        try:
            with self._wlock:
                self.ser.write(data)
            return True
        except Exception:
            self.connected = False
            return False

    async def next_msg(self, timeout: float = 0.2):
        """取一条上行消息；超时返回 None。不阻塞事件循环。"""
        try:
            return await asyncio.to_thread(self._lines.get, True, timeout)
        except queue.Empty:
            return None

    def drain(self) -> list[dict]:
        out = []
        while True:
            try:
                out.append(self._lines.get_nowait())
            except queue.Empty:
                return out
