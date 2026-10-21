"""纯函数单测：不接硬件、不开串口，验证编解码与协议解析。

跑法：  python -m tests.test_codec
"""
from __future__ import annotations

import math
import struct
import sys

import numpy as np

sys.path.insert(0, __file__.rsplit("tests", 1)[0])

from devlink import codec, proto   # noqa: E402


def check(name: str, cond: bool, detail: str = "") -> bool:
    print(f"  [{'PASS' if cond else 'FAIL'}] {name}{(' — ' + detail) if detail and not cond else ''}")
    return cond


def test_adpcm_roundtrip() -> bool:
    """正弦波编解码往返，误差应远小于信号幅度。"""
    hz, sec = 8000, 1.0
    t = np.arange(int(hz * sec)) / hz
    sig = (6000 * np.sin(2 * math.pi * 440 * t)).astype(np.int16)

    enc = codec.AdpcmEncoder()
    dec = codec.AdpcmDecoder()
    out = b""
    block = 1024
    for i in range(0, len(sig), block):
        out += dec.decode(enc.encode_block(sig[i:i + block]))
    rec = np.frombuffer(out, dtype=np.int16)
    n = min(len(rec), len(sig))
    err = float(np.abs(rec[:n].astype(int) - sig[:n].astype(int)).mean())
    return check("ADPCM 往返", len(rec) >= len(sig) and err < 600,
                 f"样本 {n}，平均误差 {err:.0f}（信号幅度 6000）")


def test_adpcm_block_independent() -> bool:
    """每块自带起始状态：跳过一块也能正确解后面那块。"""
    sig = (4000 * np.sin(np.arange(2000) / 3.0)).astype(np.int16)
    enc = codec.AdpcmEncoder()
    b1 = enc.encode_block(sig[:1000])
    b2 = enc.encode_block(sig[1000:2000])

    d = codec.AdpcmDecoder()
    pcm2 = d.decode(b2)                       # 故意不解 b1
    rec = np.frombuffer(pcm2, dtype=np.int16)
    err = float(np.abs(rec.astype(int) - sig[1000:2000].astype(int))[:len(rec)].mean())
    return check("丢块不影响后续（块内自带预测器）", err < 800, f"误差 {err:.0f}")


def test_resample() -> bool:
    pcm = (1000 * np.sin(np.arange(16000) / 10.0)).astype(np.int16).tobytes()
    out = codec.resample_mono16(pcm, 16000, 8000)
    n = len(out) // 2
    return check("重采样 16k→8k", abs(n - 8000) <= 2, f"输出 {n} 样本")


def test_prepare_for_asr() -> bool:
    # 模拟这块板子的实况：巨大直流偏置 + 小信号
    t = np.arange(16000)
    dc = -27000
    sig = (dc + 900 * np.sin(2 * np.pi * 300 * t / 16000)).astype(np.int16)
    y = codec.prepare_for_asr(sig.tobytes(), 16000)
    mean_ok = abs(float(np.mean(y))) < 0.25
    return check("去直流 + 归一化", y.size > 0 and mean_ok,
                 f"长度 {y.size}，均值 {float(np.mean(y)) if y.size else 0:.3f}")

    # 纯底噪应该被判为无声
    quiet = (dc + np.random.RandomState(0).randint(-8, 8, 16000)).astype(np.int16)
    q = codec.prepare_for_asr(quiet.tobytes(), 16000)
    return check("静音被识别为空", q.size == 0, f"返回 {q.size}")


def test_proto() -> bool:
    line = proto.encode(proto.HOST_STATUS, state="thinking", text="跑着呢")
    obj = proto.parse(line)
    ok1 = obj is not None and obj["type"] == proto.HOST_STATUS \
        and obj["payload"]["text"] == "跑着呢"

    ok2 = proto.parse(b"ESP-ROM:esp32c3-api1-20210207") is None
    ok3 = proto.parse(b'{"type":"x","payload":{}}')["payload"] == {}
    return check("协议编解码", ok1) and check("忽略日志行", ok2) and check("空 payload", ok3)


def main() -> int:
    print("== codec / proto 单测 ==")
    results = [
        test_adpcm_roundtrip(),
        test_adpcm_block_independent(),
        test_resample(),
        test_prepare_for_asr(),
        test_proto(),
    ]
    print(f"==> {sum(results)}/{len(results)} 通过")
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
