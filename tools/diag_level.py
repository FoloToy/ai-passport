"""麦克风体检：底噪 vs 说话电平，算信噪比并给判定。

用法：
    python tools/diag_level.py            # 先录 3s 静音，再录 6s 说话
    python tools/diag_level.py 8          # 说话录 8s

为什么需要它：识别不准的时候，必须先分清是
  · 信号太弱（麦克风增益不够 / 麦克风没开孔）
  · 还是环境太吵（信噪比低）
两者的修法完全不同，靠猜只会浪费轮次。
"""
import sys
import threading
import time

sys.path.insert(0, r"C:\Users\46755\WorkBuddy\ai-passport\host")

from devlink.server import _client  # noqa: E402


def main():
    sec = int(sys.argv[1]) if len(sys.argv) > 1 else 6

    print("开着设备，先别说话，录 3 秒底噪…", flush=True)
    r0 = _client({"op": "record", "sec": 3, "asr": False}, 40)
    s0 = r0.get("stats", {})
    base = s0.get("ac_rms") or 0
    print(f"  底噪 rms={base} peak={s0.get('peak')}", flush=True)

    print(f"\n现在对着设备说一句话（{sec} 秒内）…", flush=True)
    r = _client({"op": "record", "sec": sec, "asr": True}, sec + 40)
    s = r.get("stats", {})
    sig = s.get("ac_rms") or 0
    txt = r.get("text") or ""
    print(f"  说话 rms={sig} peak={s.get('peak')}", flush=True)
    print(f"  识别: {txt!r}", flush=True)

    ratio = sig / max(base, 1)
    print(f"\n信噪比: {sig} / {base} = {ratio:.1f}x", flush=True)
    if sig < 60:
        print("→ 信号太弱：检查麦克风开孔/贴膜，或把 WB_MIC_GAIN 调更高", flush=True)
    elif ratio < 3:
        print("→ 信噪比太低：环境太吵，或说话离设备太远（20-30cm 最佳）", flush=True)
    elif ratio < 6:
        print("→ 勉强可用，安静环境会更准", flush=True)
    else:
        print("→ 电平良好，识别结果反映真实能力", flush=True)


if __name__ == "__main__":
    main()
