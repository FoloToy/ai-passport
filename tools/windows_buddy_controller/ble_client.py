"""Threaded Bleak client for Windows Hardware Buddy testing."""

import asyncio
from dataclasses import dataclass
import queue
import threading
from typing import Callable, List, Optional

if __package__:
    from .protocol import LineDecoder
else:
    from protocol import LineDecoder

NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
NUS_TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"


def split_for_write(payload: bytes, chunk_size: int) -> List[bytes]:
    if chunk_size <= 0:
        raise ValueError("chunk_size must be positive")
    return [payload[index:index + chunk_size]
            for index in range(0, len(payload), chunk_size)]


@dataclass(frozen=True)
class DeviceInfo:
    name: str
    address: str


class BleWorker:
    """Owns a Bleak asyncio loop in a background thread."""

    def __init__(self, event_callback: Callable[[str, object], None]) -> None:
        self._callback = event_callback
        self._thread: Optional[threading.Thread] = None
        self._loop: Optional[asyncio.AbstractEventLoop] = None
        self._commands: Optional[asyncio.Queue] = None
        self._client = None
        self._discovered = {}
        self._decoder = LineDecoder()

    def start(self) -> None:
        if self._thread and self._thread.is_alive():
            return
        self._thread = threading.Thread(target=self._thread_main, daemon=True,
                                        name="buddy-ble")
        self._thread.start()

    def scan(self) -> None:
        self._submit(("scan", None))

    def connect(self, address: str) -> None:
        self._submit(("connect", address))

    def disconnect(self) -> None:
        self._submit(("disconnect", None))

    def send(self, payload: bytes) -> None:
        self._submit(("send", payload))

    def stop(self) -> None:
        self._submit(("stop", None))

    def _submit(self, command: object) -> None:
        self.start()
        if self._loop is None or self._commands is None:
            for _ in range(100):
                if self._loop is not None and self._commands is not None:
                    break
                threading.Event().wait(0.01)
        if self._loop is not None and self._commands is not None:
            self._loop.call_soon_threadsafe(self._commands.put_nowait, command)

    def _thread_main(self) -> None:
        asyncio.run(self._run())

    async def _run(self) -> None:
        self._loop = asyncio.get_running_loop()
        self._commands = asyncio.Queue()
        while True:
            command, value = await self._commands.get()
            try:
                if command == "scan":
                    await self._scan()
                elif command == "connect":
                    await self._connect(str(value))
                elif command == "disconnect":
                    await self._disconnect()
                elif command == "send":
                    await self._send(bytes(value))
                elif command == "stop":
                    await self._disconnect()
                    return
            except Exception as error:  # Bleak errors differ by Windows version.
                self._callback("error", str(error))

    async def _scan(self) -> None:
        from bleak import BleakScanner

        self._callback("status", "Scanning...")
        discovered = await BleakScanner.discover(timeout=5.0,
                                                 service_uuids=[NUS_SERVICE_UUID])
        devices = [DeviceInfo(device.name or "(unnamed)", device.address)
                   for device in discovered
                   if (device.name or "").startswith("Claude")]
        self._discovered = {device.address: device for device in discovered}
        self._callback("devices", devices)
        self._callback("status", "Scan complete")

    async def _connect(self, address: str) -> None:
        from bleak import BleakClient

        await self._disconnect()
        self._callback(
            "status",
            "Connecting/pairing (enter the device passkey in Windows)...",
        )
        target = self._discovered.get(address, address)
        self._client = BleakClient(target, disconnected_callback=self._on_disconnect,
                                   pair=True, timeout=60.0)
        await self._client.connect()
        await self._client.start_notify(NUS_TX_UUID, self._on_notification)
        self._callback("connected", True)
        self._callback("status", "Connected and notifications enabled")

    async def _disconnect(self) -> None:
        client, self._client = self._client, None
        if client is not None:
            if client.is_connected:
                try:
                    await client.stop_notify(NUS_TX_UUID)
                except Exception:
                    pass
                await client.disconnect()
        self._callback("connected", False)

    async def _send(self, payload: bytes) -> None:
        if self._client is None or not self._client.is_connected:
            raise RuntimeError("device is not connected")
        characteristic = self._client.services.get_characteristic(NUS_RX_UUID)
        if characteristic is None:
            raise RuntimeError("device does not expose the Hardware Buddy RX characteristic")
        limit = getattr(characteristic, "max_write_without_response_size", 20)
        for chunk in split_for_write(payload, max(20, int(limit))):
            await self._client.write_gatt_char(NUS_RX_UUID, chunk, response=False)
        self._callback("tx", payload.decode("utf-8", errors="replace").rstrip())

    def _on_notification(self, _sender: object, data: bytearray) -> None:
        try:
            for line in self._decoder.feed(bytes(data)):
                self._callback("rx", line)
        except (UnicodeDecodeError, ValueError) as error:
            self._callback("error", "Invalid notification: " + str(error))

    def _on_disconnect(self, _client: object) -> None:
        self._client = None
        self._callback("connected", False)
        self._callback("status", "Disconnected")
