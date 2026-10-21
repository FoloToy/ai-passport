"""识别子进程：读 stdin 的 16k/16bit/单声道 PCM，把结果打成一行 JSON 写到 stdout。

单独一个文件、只用标准库 + win32com，避免把主程序的依赖拖进来。
"""
from __future__ import annotations

import io
import json
import os
import sys
import tempfile
import time
import wave

HZ = 16000
BACKEND = os.getenv("WB_ASR_BACKEND", "sense").lower()
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MODEL_DIR = os.getenv("WB_ASR_MODEL", os.path.join(
    HERE, "models", "sherpa-onnx-paraformer-zh-small-2024-03-09"))
# SenseVoice：阿里 FunASR 的中文/多语种模型，带标点与逆文本归一化（"10点"而非"十点"），
# 同一段弱信号实测比 paraformer-small 连贯得多。228MB（int8），载入约 2.2s。
SENSE_DIR = os.path.join(HERE, "models", "sense-voice")


def read_pcm() -> bytes:
    data = sys.stdin.buffer.read()
    if data[:4] == b"RIFF":
        with wave.open(io.BytesIO(data), "rb") as w:
            return w.readframes(w.getnframes())
    return data


def to_float(pcm: bytes):
    import numpy as np
    x = np.frombuffer(pcm, dtype=np.int16).astype("float32") / 32768.0
    return x - float(x.mean()) if x.size else x


# ---------------------------------------------------------------- SAPI

def split_on_silence(pcm: bytes, hz: int = HZ, frame_ms: int = 10,
                     gap_ms: int = 260, pad_ms: int = 140, max_seg_ms: int = 3600):
    """按能量在停顿处切段。

    为什么要切：Windows 桌面听写引擎（MS-2052-80-DESK）对一段连续语音只输出
    一个结果，大约 3.5 秒之后就丢内容 —— 实测"请把今天的实验记录整理成表格，
    然后告诉我结论"只回来前半句。切成短段逐段识别再拼接，就不会被截。
    """
    import numpy as np

    if not pcm:
        return [b""]
    x = np.frombuffer(pcm, dtype=np.int16).astype(np.float32)
    x = x - float(x.mean())
    fl = max(1, hz * frame_ms // 1000)
    n = x.size // fl
    if n <= 2:
        return [pcm]
    frames = x[:n * fl].reshape(n, fl)
    rms = np.sqrt((frames ** 2).mean(axis=1))
    # 阈值：取较高分位与底噪的折中，避免把气声判成静音
    thr = max(float(np.percentile(rms, 25)) * 1.8, 120.0)
    voiced = rms > thr

    gap = max(1, gap_ms // frame_ms)
    pad = max(1, pad_ms // frame_ms)
    max_seg = max(1, max_seg_ms // frame_ms)

    segs = []
    i = 0
    while i < n:
        if not voiced[i]:
            i += 1
            continue
        start = max(0, i - pad)
        j = i
        last_voiced = i
        while j < n:
            if voiced[j]:
                last_voiced = j
            elif j - last_voiced >= gap:      # 停顿够长 → 断开
                break
            if j - start >= max_seg:          # 太长也没有收益，强制断开
                last_voiced = j
                break
            j += 1
        end = min(n, last_voiced + 1 + pad)
        segs.append(pcm[start * fl * 2:end * fl * 2])
        i = end
    return segs or [pcm]


def run_sapi(pcm: bytes) -> str:
    # 分段后逐段识别；每段自带临时文件，复用同一个识别器
    segs = split_on_silence(pcm)
    if len(segs) > 1:
        parts = [_sapi_once(s) for s in segs if len(s) >= hz_bytes(0.25)]
        return "".join(p.strip() for p in parts)
    return _sapi_once(pcm)


def hz_bytes(seconds: float) -> int:
    """该时长对应的最少字节数（用于过滤过短片段）。"""
    return int(HZ * seconds) * 2


def _sapi_once(pcm: bytes) -> str:
    import pythoncom
    import win32com.client as wc
    from win32com.client import dynamic

    tmp = tempfile.mktemp(suffix=".wav")
    with wave.open(tmp, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(HZ)
        w.writeframes(pcm)

    pythoncom.CoInitialize()
    texts: list[str] = []
    state = {"last": 0.0}

    class Handler:
        def OnRecognition(self, *args):
            result = args[-1]
            state["last"] = time.time()
            # 必须用 dynamic.Dispatch：gen_py 生成的 ISpeechRecoResult 里
            # 没有 RecognitionText 这个早绑定成员，直接 wc.Dispatch 会 AttributeError。
            try:
                texts.append(dynamic.Dispatch(result).RecognitionText or "")
            except Exception:
                try:
                    texts.append(dynamic.Dispatch(result).PhraseInfo.GetText() or "")
                except Exception:
                    pass

    try:
        rec = wc.Dispatch("SAPI.SpInprocRecognizer")
        stream = wc.Dispatch("SAPI.SpFileStream")
        stream.Open(tmp, 0, False)                 # SSFMOpenForRead
        rec.AudioInputStream = stream
        ctx = rec.CreateRecoContext()
        handler = wc.WithEvents(ctx, Handler)       # 必须留引用，否则被 GC 掉收不到事件
        ctx.CreateGrammar().DictationSetState(1)    # SGDSActive

        # SAPI 吃音频大约就是实时速度，时限必须 ≥ 音频时长（设短了会把后半句整段丢掉）。
        # 但每段通常只出一个结果，所以"拿到结果后再安静 0.45s"就收尾，不必干等满。
        deadline = time.time() + len(pcm) / 2.0 / HZ + 1.2
        while time.time() < deadline:
            pythoncom.PumpWaitingMessages()
            time.sleep(0.02)
            if texts and (time.time() - state["last"]) > 0.45:
                break
        del handler
    finally:
        try:
            os.remove(tmp)
        except OSError:
            pass
    return "".join(t.strip() for t in texts)


# ---------------------------------------------------------------- sherpa-onnx

_SHERPA = None


def _sherpa():
    """模型只载一次（约 2.4s），之后每句只要 0.1s 量级。"""
    global _SHERPA
    if _SHERPA is not None:
        return _SHERPA
    import sherpa_onnx

    onnx = ""
    for name in ("model.int8.onnx", "model.onnx"):
        cand = os.path.join(MODEL_DIR, name)
        if os.path.exists(cand):
            onnx = cand
            break
    tokens = os.path.join(MODEL_DIR, "tokens.txt")
    if not onnx or not os.path.exists(tokens):
        raise FileNotFoundError(f"缺模型：{MODEL_DIR}")
    # 不要传 decoding_method：paraformer 只支持 greedy，传别的会打警告。
    _SHERPA = sherpa_onnx.OfflineRecognizer.from_paraformer(
        paraformer=onnx, tokens=tokens, num_threads=4, sample_rate=HZ, feature_dim=80,
    )
    return _SHERPA


def run_sherpa(pcm: bytes) -> str:
    rec = _sherpa()
    s = rec.create_stream()
    s.accept_waveform(HZ, to_float(pcm))
    rec.decode_stream(s)
    return (s.result.text or "").strip()


# ---------------------------------------------------------------- SenseVoice

_SENSE = None


def _sense():
    """SenseVoice 也只需载一次（约 2.2s）。"""
    global _SENSE
    if _SENSE is not None:
        return _SENSE
    import sherpa_onnx

    onnx = ""
    for name in ("model.int8.onnx", "model.onnx"):
        cand = os.path.join(SENSE_DIR, name)
        if os.path.exists(cand):
            onnx = cand
            break
    tokens = os.path.join(SENSE_DIR, "tokens.txt")
    if not onnx or not os.path.exists(tokens):
        raise FileNotFoundError(f"缺 SenseVoice 模型：{SENSE_DIR}")
    # 词表要校验：下载中断会留下"大小对但全是 NUL"的文件，
    # 表现为 decode 时 `invalid unordered_map<K, T> key`，很难从结果反推。
    if os.path.getsize(tokens) < 10000:
        raise ValueError(f"tokens.txt 异常偏小（{os.path.getsize(tokens)} B）")
    _SENSE = sherpa_onnx.OfflineRecognizer.from_sense_voice(
        model=onnx, tokens=tokens, num_threads=4,
        use_itn=True,       # 逆文本归一化：数字/单位写成自然形式
        debug=False,
    )
    return _SENSE


def run_sense(pcm: bytes) -> str:
    rec = _sense()
    import numpy as np

    x = np.frombuffer(pcm, dtype=np.int16).astype(np.float32) / 32768.0
    s = rec.create_stream()
    s.accept_waveform(HZ, x)
    rec.decode_stream(s)
    return (s.result.text or "").strip()


# ---------------------------------------------------------------- 静音裁剪

_VAD = None
VAD_PATH = os.path.join(os.path.dirname(MODEL_DIR), "silero_vad.onnx")
VAD_MODE = os.getenv("WB_VAD", "energy").lower()   # energy | silero | off


def trim_silence(pcm: bytes, hz: int = HZ, frame_ms: int = 20,
                 rel_db: float = -35.0, pad_ms: int = 250) -> bytes:
    """只裁掉首尾的静音/底噪，中间一律保留。

    为什么不用 silero VAD 全量切分：实测它会把开头的轻声字吃掉
    （"帮我把这周的…" 变成 "把这周的…"），而且没有 padding 参数可调。
    录到环境音时确实能把噪音切掉，但代价是丢字——对下命令来说丢字更糟。

    这个实现可预测：算出每 20ms 一帧的能量，找到第一个和最后一个
    超过「峰值 -35dB」的帧，两端各留 250ms 余量，中间原样保留。
    """
    import numpy as np

    if not pcm:
        return pcm
    x = np.frombuffer(pcm, dtype=np.int16).astype(np.float32)
    n = max(1, int(hz * frame_ms / 1000))
    frames = x.size // n
    if frames < 5:
        return pcm

    e = np.sqrt((x[:frames * n].reshape(frames, n) ** 2).mean(axis=1))
    peak = float(e.max())
    if peak < 25.0:                       # 全程几乎没声音，交给上层判空
        return b""

    thr = peak * (10.0 ** (rel_db / 20.0))
    idx = np.where(e > thr)[0]
    if idx.size == 0:
        return pcm

    pad = int(hz * pad_ms / 1000)
    start = max(0, int(idx[0]) * n - pad)
    end = min(x.size, (int(idx[-1]) + 1) * n + pad)
    return x[start:end].astype(np.int16).tobytes()


def vad_trim(pcm: bytes) -> bytes:
    """按 WB_VAD 配置做语音段裁剪。"""
    import numpy as np

    mode = VAD_MODE
    if mode == "off":
        return pcm
    if mode == "energy":
        return trim_silence(pcm)

    # silero：能切掉人声噪音，但可能吃掉开头的轻声字（默认不用）
    global _VAD
    if _VAD is None:
        if not os.path.exists(VAD_PATH):
            return trim_silence(pcm)
        import sherpa_onnx

        cfg = sherpa_onnx.VadModelConfig()
        cfg.silero_vad.model = VAD_PATH
        cfg.silero_vad.min_silence_duration = 0.35
        cfg.silero_vad.min_speech_duration = 0.15
        cfg.silero_vad.threshold = 0.35
        cfg.sample_rate = HZ
        _VAD = sherpa_onnx.VoiceActivityDetector(cfg, buffer_size_in_seconds=40)

    try:
        x = to_float(pcm)
        step = HZ // 4
        for i in range(0, len(x), step):
            _VAD.accept_waveform(x[i:i + step])
        _VAD.flush()
        keep = []
        while not _VAD.empty():
            keep.append(np.array(_VAD.front.samples, dtype=np.float32))
            _VAD.pop()
        if not keep:
            return b""
        y = np.concatenate(keep)
        return np.clip(y * 32767.0, -32768, 32767).astype(np.int16).tobytes()
    except Exception as e:
        print(f"[VAD] 失败，退回能量裁剪：{e}", file=sys.stderr, flush=True)
        return trim_silence(pcm)


# ---------------------------------------------------------------- faster-whisper

def run_whisper(pcm: bytes) -> str:
    from faster_whisper import WhisperModel

    model = WhisperModel(os.getenv("WB_WHISPER", "small"), device="cpu",
                         compute_type="int8", cpu_threads=4, local_files_only=True)
    segs, _ = model.transcribe(to_float(pcm), language="zh", beam_size=3, vad_filter=True)
    return "".join(s.text for s in segs).strip()


def recognize(pcm: bytes) -> dict:
    if len(pcm) < 3200:
        return {"text": ""}
    order = (["sense", "sherpa", "sapi", "whisper"] if BACKEND == "auto"
             else [BACKEND])
    fns = {"sapi": run_sapi, "sherpa": run_sherpa, "sense": run_sense,
           "whisper": run_whisper}
    errors = []
    raw_len = len(pcm) // 2
    for be in order:
        try:
            # 先切掉非语音段：录音窗口里的环境音会被当成待识别内容，污染结果。
            speech_pcm = vad_trim(pcm) if be in ("sherpa", "sense", "whisper") else pcm
            if not speech_pcm:
                print(f"[VAD] 全是静音/噪音，跳过识别（原始 {raw_len / HZ:.1f}s）",
                      file=sys.stderr, flush=True)
                return {"text": "", "backend": be, "vad": "empty"}
            print(f"[VAD] {raw_len / HZ:.1f}s → {len(speech_pcm) / 2 / HZ:.1f}s",
                  file=sys.stderr, flush=True)
            return {"text": fns[be](speech_pcm), "backend": be}
        except Exception as e:
            errors.append(f"{be}: {type(e).__name__}: {e}")
    return {"error": " | ".join(errors)}


def main() -> None:
    """一次性模式：从 stdin 读整段 PCM，输出一行 JSON。"""
    print(json.dumps(recognize(read_pcm()), ensure_ascii=False))


def serve() -> None:
    """常驻模式：4 字节大端长度 + PCM，循环处理。

    父进程用这个模式，省掉每次识别都要新起 Python 的 1.4s。
    """
    import struct
    sys.stdout.reconfigure(encoding="utf-8")
    out = sys.stdout
    while True:
        head = sys.stdin.buffer.read(4)
        if len(head) < 4:
            return
        n = struct.unpack(">I", head)[0]
        pcm = b""
        while len(pcm) < n:
            chunk = sys.stdin.buffer.read(n - len(pcm))
            if not chunk:
                return
            pcm += chunk
        out.write(json.dumps(recognize(pcm), ensure_ascii=False) + "\n")
        out.flush()


if __name__ == "__main__":
    if "--serve" in sys.argv:
        serve()
    else:
        main()
