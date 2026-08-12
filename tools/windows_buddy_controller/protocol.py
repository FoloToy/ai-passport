"""Hardware Buddy newline-delimited JSON protocol helpers."""

from dataclasses import dataclass
import json
from typing import Any, Dict, Iterable, List, Optional


def _line(payload: Dict[str, Any]) -> bytes:
    return (json.dumps(payload, ensure_ascii=False, separators=(",", ":")) + "\n").encode("utf-8")


def heartbeat_payload(total: int, running: int, waiting: int, message: str,
                      entries: Iterable[str], tokens: int, tokens_today: int,
                      prompt: Optional[Dict[str, str]] = None) -> Dict[str, Any]:
    payload: Dict[str, Any] = {
        "total": int(total),
        "running": int(running),
        "waiting": int(waiting),
        "msg": message,
        "entries": list(entries),
        "tokens": int(tokens),
        "tokens_today": int(tokens_today),
    }
    if prompt:
        payload["prompt"] = {
            "id": prompt["id"],
            "tool": prompt.get("tool", ""),
            "hint": prompt.get("hint", ""),
        }
    return payload


def build_heartbeat(**kwargs: Any) -> bytes:
    return _line(heartbeat_payload(**kwargs))


def build_time_sync(epoch_seconds: int, timezone_offset_seconds: int) -> bytes:
    return _line({"time": [int(epoch_seconds), int(timezone_offset_seconds)]})


def build_owner(name: str) -> bytes:
    return _line({"cmd": "owner", "name": name})


def build_name(name: str) -> bytes:
    return _line({"cmd": "name", "name": name})


def build_status_request() -> bytes:
    return _line({"cmd": "status"})


def build_unpair() -> bytes:
    return _line({"cmd": "unpair"})


@dataclass(frozen=True)
class DeviceMessage:
    kind: str
    payload: Dict[str, Any]


def parse_device_message(line: str) -> DeviceMessage:
    payload = json.loads(line)
    if not isinstance(payload, dict):
        raise ValueError("device message must be a JSON object")
    if payload.get("cmd") == "permission":
        if not isinstance(payload.get("id"), str) or not payload["id"]:
            raise ValueError("permission id is required")
        if payload.get("decision") not in ("once", "deny"):
            raise ValueError("invalid permission decision")
        return DeviceMessage("permission", payload)
    if payload.get("ack") == "status":
        return DeviceMessage("status", payload)
    if isinstance(payload.get("ack"), str):
        return DeviceMessage("ack", payload)
    return DeviceMessage("unknown", payload)


class LineDecoder:
    def __init__(self, max_line_bytes: int = 4096) -> None:
        self._maximum = max_line_bytes
        self._buffer = bytearray()
        self._discarding = False
        self.overflow_count = 0

    def feed(self, data: bytes) -> List[str]:
        lines: List[str] = []
        for byte in data:
            if byte == 0x0A:
                if self._discarding:
                    self._discarding = False
                elif self._buffer:
                    if self._buffer[-1:] == b"\r":
                        del self._buffer[-1:]
                    lines.append(self._buffer.decode("utf-8"))
                self._buffer.clear()
            elif self._discarding:
                continue
            elif len(self._buffer) >= self._maximum:
                self._buffer.clear()
                self._discarding = True
                self.overflow_count += 1
            else:
                self._buffer.append(byte)
        return lines
