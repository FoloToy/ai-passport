"""自动增益校准：录音测峰值，自动选一个"够响但不削顶"的增益。

为什么要做：固定 30dB 在不同环境下结果差异很大——
同一档实测过 peak=6161（安全）和 peak=32768（削顶）。
削顶的音频波形被削平，识别必然很差，而且从识别结果看不出是增益问题。

判据：
  · peak >= 30000 → 削顶，降 3dB
  · peak < 800     → 太弱，升 3dB
  · 其余           → 可用，停下
最多迭代 6 次，每次录 1.2 秒（不识别，只测电平）。
"""
import sys
import time

sys.path.insert(0, r"C:\Users\46755\WorkBuddy\ai-passport\host")

from devlink.server import _client  # noqa: E402

CLIP = 30000
WEAK = 800
STEP = 3.0


def probe(db):
    _client({"op": "gain", "db": db}, 8)
    time.sleep(0.5)
    r = _client({"op": "record", "sec": 1.2, "asr": False}, 30)
    st = r.get("stats") or {}
    return st.get("peak") or 0, st.get("ac_rms") or 0


def main():
    print("自动增益校准：请保持环境正常音量（不说话也可以，读的是底噪/环境）", flush=True)
    print(f"{'增益':>7} {'peak':>7} {'rms':>7}  判定", flush=True)

    db = 30.0
    best = None
    for i in range(6):
        peak, rms = probe(db)
        if peak >= CLIP:
            verdict = "削顶 → 降"
            if best is None:
                best = (peak, rms, db)
        elif peak < WEAK:
            verdict = "太弱 → 升"
        else:
            verdict = "可用 ✅"
        print(f"{db:6.0f}dB {peak:>7} {rms:>7}  {verdict}", flush=True)

        if verdict.startswith("可用"):
            if best is None or peak > best[0]:
                best = (peak, rms, db)
            break

        if peak >= CLIP:
            db -= STEP
        else:
            db += STEP
        if db < 0:
            db = 0.0
        if db > 30:
            db = 30.0
        time.sleep(0.3)

    if best:
        print(f"\n采用 {best[2]:.0f}dB（peak={best[0]} rms={best[1]}）", flush=True)
        _client({"op": "gain", "db": best[2]}, 8)
    else:
        print("\n没找到理想档位，保持当前", flush=True)

    print("\n提示：如果噪声环境变化大，重跑本脚本即可", flush=True)


if __name__ == "__main__":
    main()
