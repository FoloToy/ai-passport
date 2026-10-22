"""语音：离线识别（子进程）+ 语音合成（8k PCM）。

为什么识别要开子进程
--------------------
本机 ctranslate2 / torch 的原生库加载即崩（0xC0000005，venv 3.13 与 D:\\Python311
都一样），进程直接没了。放进子进程后最坏情况只是这一句没识别出来，服务还活着。

识别后端（WB_ASR_BACKEND）
    sapi     Windows 自带中文听写引擎（离线、零下载、实测近满字）—— 默认
    sherpa   sherpa-onnx + Paraformer（需要放模型）
    whisper  faster-whisper（需要能用的 ctranslate2）
"""
from __future__ import annotations

import asyncio
import json
import os
import subprocess
import sys
import tempfile
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
VOICE_HZ = 8000          # 设备播报采样率：人声够用，串口压力减半
ASR_HZ = 16000

# SAPI SpAudioFormat.Type = 6  →  8kHz / 16bit / 单声道
_SAFT_8K_16BIT_MONO = 6


# ---------------------------------------------------------------- 识别

def _worker_path() -> str:
    return os.path.join(HERE, "asr_worker.py")


class _Worker:
    """常驻识别子进程。

    一次性子进程要付 ~1.4s 的 Python 冷启动，而一句话本身才 3 秒。
    改成常驻：4 字节大端长度 + PCM 进去，一行 JSON 出来。崩了就重启。
    """

    def __init__(self) -> None:
        self.p = None
        self._lock = None

    def _spawn(self):
        import struct  # noqa: F401  (仅表明协议)
        self.p = subprocess.Popen(
            [sys.executable, _worker_path(), "--serve"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            env={**os.environ, "PYTHONIOENCODING": "utf-8"},
        )
        return self.p

    def ask(self, pcm: bytes, timeout: float = 120.0) -> dict:
        import struct
        import threading
        if self._lock is None:
            self._lock = threading.Lock()
        with self._lock:
            for attempt in (0, 1):
                try:
                    p = self.p if (self.p and self.p.poll() is None) else self._spawn()
                    p.stdin.write(struct.pack(">I", len(pcm)) + pcm)
                    p.stdin.flush()
                    line = p.stdout.readline()
                    if not line:
                        raise BrokenPipeError("worker 无输出")
                    return json.loads(line.decode("utf-8", "replace"))
                except Exception as e:
                    self.p = None
                    if attempt == 1:
                        return {"error": f"{type(e).__name__}: {e}"}
            return {"error": "unreachable"}

    def close(self) -> None:
        if self.p:
            try:
                self.p.terminate()
            except Exception:
                pass
            self.p = None


WORKER = _Worker()


def transcribe(pcm16k: bytes, timeout: float = 120.0) -> str:
    """同步识别。失败返回空串，绝不抛异常（语音链路不该拖垮服务）。

    识别前必须做语音增强：这块板子的 ES8311 采集带 ≈ -0.84 FS 的直流偏置，
    而且信号很弱（15dB 增益下 peak 只有 200 左右）。
    不除直流、不做高通、不做归一化就直接喂识别器，结果会是
    "好的我喝酒高最起码吧帮我把…" 这种——前半段全是底噪幻觉。
    """
    if len(pcm16k) < 3200:                 # < 0.1s
        return ""
    import time as _t
    t0 = _t.time()

    from . import codec
    clean = codec.prepare_for_asr(pcm16k, 16000)
    if clean.size == 0:
        print("[ASR] 只有底噪，跳过", file=sys.stderr, flush=True)
        return ""
    import numpy as np
    pcm = np.clip(clean * 32767.0, -32768, 32767).astype(np.int16).tobytes()

    obj = WORKER.ask(pcm, timeout)
    if "error" in obj:
        print(f"[ASR] 失败：{obj['error'][:300]}", file=sys.stderr, flush=True)
        return ""
    text = (obj.get("text") or "").strip()
    print(f"[ASR] {_t.time() - t0:.1f}s [{obj.get('backend')}] → {text!r}",
          file=sys.stderr, flush=True)
    return text


async def transcribe_async(pcm16k: bytes) -> str:
    return await asyncio.to_thread(transcribe, pcm16k)


# ---------------------------------------------------------------- 合成

def tts_pcm(text: str, hz: int = VOICE_HZ) -> bytes:
    """文字 → 单声道 16bit PCM。SAPI 优先（离线），失败回落 edge-tts。"""
    text = (text or "").strip()
    if not text:
        return b""
    try:
        pcm = _sapi_pcm(text, hz)
        if pcm:
            return pcm
    except Exception as e:
        print(f"[TTS] SAPI 失败：{e}", file=sys.stderr, flush=True)
    try:
        return _edge_pcm(text, hz)
    except Exception as e:
        print(f"[TTS] edge-tts 也失败：{e}", file=sys.stderr, flush=True)
        return b""


def _sapi_pcm(text: str, hz: int) -> bytes:
    import win32com.client as wc
    import pythoncom

    pythoncom.CoInitialize()
    tmp = tempfile.mktemp(suffix=".wav")
    voice = wc.Dispatch("SAPI.SpVoice")
    stream = wc.Dispatch("SAPI.SpFileStream")
    fmt = wc.Dispatch("SAPI.SpAudioFormat")

    want = (os.getenv("WB_SAPI_VOICE") or "").strip()
    try:
        voices = [v for v in voice.GetVoices()]
        pick = None
        for v in voices:
            desc = v.GetDescription()
            if want and want.lower() in desc.lower():
                pick = v
                break
            if not want and any(k in desc for k in ("Chinese", "中文", "Huihui", "Yaoyao", "Kangkang")):
                pick = v
        if pick is not None:
            voice.Voice = pick
    except Exception:
        pass

    if hz == 8000:
        try:
            fmt.Type = _SAFT_8K_16BIT_MONO
            stream.Format = fmt
        except Exception:
            pass

    stream.Open(tmp, 3)                     # SSFMCreateForWrite
    voice.AudioOutputStream = stream
    voice.Speak(text)
    stream.Close()

    try:
        with wave.open(tmp, "rb") as w:
            sr, ch, sw = w.getframerate(), w.getnchannels(), w.getsampwidth()
            data = w.readframes(w.getnframes())
    finally:
        try:
            os.remove(tmp)
        except OSError:
            pass

    if sw != 2 or not data:
        return b""
    if ch > 1:
        import numpy as np
        a = np.frombuffer(data, dtype=np.int16).reshape(-1, ch)[:, 0]
        data = a.copy().tobytes()
    if sr != hz:
        from .codec import resample_mono16
        data = resample_mono16(data, sr, hz)
    return data


def _edge_pcm(text: str, hz: int) -> bytes:
    """网络兜底：edge-tts 合成 mp3，PyAV 解成目标采样率单声道。"""
    import asyncio as aio
    import edge_tts
    import av

    mp3 = tempfile.mktemp(suffix=".mp3")
    voice = os.getenv("WB_EDGE_VOICE", "zh-CN-XiaoxiaoNeural")
    aio.run(edge_tts.Communicate(text, voice).save(mp3))
    chunks = []
    with av.open(mp3) as c:
        rs = av.AudioResampler(format="s16", layout="mono", rate=hz)
        for frame in c.decode(c.streams.audio[0]):
            for f in rs.resample(frame):
                chunks.append(bytes(f.planes[0])[: f.samples * 2])
    try:
        os.remove(mp3)
    except OSError:
        pass
    return b"".join(chunks)
