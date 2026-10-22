"""分析 last_raw.wav 的削顶分布、分位数与静音段，判断是"整体过载"还是"偶发爆音"。"""
import os
import sys
import wave

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
p = os.path.join(ROOT, "host", "recordings", "last_raw.wav")
if not os.path.exists(p):
    print("没有录音文件")
    sys.exit(1)

w = wave.open(p, "rb")
x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float32)
hz = w.getframerate()
print(f"文件 {w.getnframes()} 采样 / {w.getnframes()/hz:.2f}s @{hz}Hz")
print(f"rms={np.sqrt((x**2).mean()):.0f}  peak={int(np.abs(x).max())}")

full = int((np.abs(x) >= 32700).sum())
print(f"满幅样本 {full} / {x.size} = {full*1000/x.size:.1f}‰")

# 每 100ms 一格的 rms 与削顶数
step = hz // 10
print("\n每 100ms：rms / 削顶样本")
for i in range(0, len(x) - step, step):
    seg = x[i:i + step]
    r = float(np.sqrt((seg ** 2).mean()))
    c = int((np.abs(seg) >= 32700).sum())
    bar = "#" * min(40, int(r / 200))
    print(f"  {i/hz:5.1f}s rms={r:6.0f} clip={c:3d} {bar}")

# 分位数：如果 99% 分位也接近满幅，说明整体过载；否则是偶发尖峰
q = np.percentile(np.abs(x), [50, 90, 99, 99.9])
print("\n绝对值分位：50%={:.0f} 90%={:.0f} 99%={:.0f} 99.9%={:.0f}".format(*q))
if q[2] > 20000:
    print("判定：整体过载（增益太高），应降低增益")
elif q[3] > 30000:
    print("判定：偶发爆音/尖峰，主体信号正常，可以考虑软削顶或截掉首尾")
else:
    print("判定：无明显削顶")
