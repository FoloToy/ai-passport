"""同一段音频上对比 Paraformer 与 SenseVoice 的识别结果与耗时。"""
import os
import sys
import time

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "host"))

from devlink import codec, speech

SV_DIR = os.path.join(ROOT, "host", "models", "sense-voice")

CASES = [
    ("合成-清晰", "帮我把这周的实验数据整理成表格"),
    ("合成-较长", "明天上午十点提醒我去三楼会议室开会"),
    ("合成-专业", "第三组复孔偏差偏大，建议重新跑一次"),
]

print("=== 1. 载入 SenseVoice ===", flush=True)
import sherpa_onnx as so

t0 = time.time()
sv = so.OfflineRecognizer.from_sense_voice(
    model=os.path.join(SV_DIR, "model.int8.onnx"),
    tokens=os.path.join(SV_DIR, "tokens.txt"),
    num_threads=4,
    use_itn=True,          # 逆文本归一化：数字/标点更自然
    debug=False,
)
print(f"载入耗时 {time.time()-t0:.1f}s", flush=True)


def run_sv(pcm16k: bytes) -> str:
    x = np.frombuffer(pcm16k, dtype=np.int16).astype(np.float32) / 32768.0
    s = sv.create_stream()
    s.accept_waveform(16000, x)
    sv.decode_stream(s)
    return (s.result.text or "").strip()


print("\n=== 2. 逐条对比 ===", flush=True)
for label, text in CASES:
    pcm = speech.tts_pcm(text, 16000)
    print(f"\n[{label}] 原文: {text}", flush=True)

    t0 = time.time()
    a = speech.transcribe(pcm)
    ta = time.time() - t0

    t0 = time.time()
    b = run_sv(pcm)
    tb = time.time() - t0

    print(f"  Paraformer  ({ta:4.1f}s): {a}", flush=True)
    print(f"  SenseVoice  ({tb:4.1f}s): {b}", flush=True)


print("\n=== 3. 真实设备录音（若存在）===", flush=True)
for name in ("last_voice.wav", "last_raw.wav"):
    p = os.path.join(ROOT, "host", "recordings", name)
    if not os.path.exists(p):
        continue
    import wave

    w = wave.open(p, "rb")
    pcm = w.readframes(w.getnframes())
    pcm = codec.trim_transient(pcm, w.getframerate())
    x = np.frombuffer(pcm, dtype=np.int16).astype(np.float32)
    print(f"\n[{name}] {x.size/16000:.1f}s rms={np.sqrt((x**2).mean()):.0f}", flush=True)
    t0 = time.time()
    a = speech.transcribe(pcm)
    print(f"  Paraformer  ({time.time()-t0:4.1f}s): {a}", flush=True)
    t0 = time.time()
    b = run_sv(pcm)
    print(f"  SenseVoice  ({time.time()-t0:4.1f}s): {b}", flush=True)

speech.WORKER.close()
