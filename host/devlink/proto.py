"""设备 ↔ PC 的消息约定。

传输：换行分隔的 UTF-8 JSON（USB-CDC 串口，115200 无所谓，CDC 不跑波特率）。
方向约定：`DEV_*` 是设备发出，`HOST_*` 是 PC 发出。

不在这里做解析之外的任何判断——保持纯函数，方便 host 单测。
"""
from __future__ import annotations

import json

# ---------------------------------------------------------------- 设备 → PC

DEV_KEY = "wb_key_event"           # {button: up|down|ok, event: click|press|release}
DEV_ANSWER = "wb_answer"           # {id, v: confirm|cancel}
DEV_AUDIO = "wb_audio"             # {s: seq, b: base64([seq][len:2][adpcm block])}
DEV_AUDIO_END = "wb_audio_end"     # {}
DEV_PONG = "wb_pong"               # {fw, proto}

# ---------------------------------------------------------------- PC → 设备

HOST_STATUS = "wb_status"          # {state: idle|thinking|waiting|alert, text}
HOST_HEADER = "wb_header"          # {text}                     标题栏（多会话标签）
HOST_REPLY = "wb_reply"            # {text}                     写入对话记录
HOST_NOTIFY = "wb_notification"    # {title, body, priority}
HOST_ASK = "wb_ask"                # {id, text}                 弹确认框
HOST_CLEAR = "wb_clear_screen"     # {}
HOST_PING = "wb_ping"              # {}
HOST_HEARTBEAT = "wb_heartbeat"    # {}                         续命（固件 30s 判离线）
HOST_TTS_BEGIN = "wb_tts_begin"    # {hz, a: 1=adpcm}
HOST_TTS_CHUNK = "wb_tts_chunk"    # {s, b: base64(adpcm block)}
HOST_TTS_END = "wb_tts_end"        # {}
HOST_TTS_STOP = "wb_tts_stop"      # {}
HOST_DEBUG_TALK = "wb_debug_talk"  # {on: bool}                 远程开关麦克风
HOST_MIC_GAIN = "wb_mic_gain"      # {db: float}                麦克风输入增益(dB)

# 固件 30s 收不到任何消息就显示"PC 离线"，所以心跳要更密
HEARTBEAT_SEC = 10.0


def encode(kind: str, **payload) -> bytes:
    """组装一行消息。payload 为空时仍然带空对象，固件端两者都能解析。"""
    obj = {"type": kind, "payload": payload}
    return (json.dumps(obj, ensure_ascii=False, separators=(",", ":")) + "\n").encode("utf-8")


def parse(line: bytes | str) -> dict | None:
    """解析一行。非 JSON / 非协议消息返回 None（串口上混着日志，必须容忍）。"""
    if isinstance(line, bytes):
        line = line.decode("utf-8", "replace")
    line = line.strip()
    if not line.startswith("{"):
        return None
    try:
        obj = json.loads(line)
    except (ValueError, UnicodeDecodeError):
        return None
    if not isinstance(obj, dict) or "type" not in obj:
        return None
    obj.setdefault("payload", {})
    return obj
