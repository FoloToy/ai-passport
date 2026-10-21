"""测固件→PC 的原始上行吞吐（不经过 devlink，直接读串口）。

用法：python tools/measure_uplink.py [秒数]
"""
import sys
import time

import serial

PORT = "COM4"
SEC = float(sys.argv[1]) if len(sys.argv) > 1 else 5.0

s = serial.Serial(PORT, 115200, timeout=0)
print(f"打开 {PORT}")

# 记录：开始 -> 开麦 -> 读 N 秒 -> 关麦 -> 读到静默
s.write(b'{"type":"wb_debug_talk","payload":{"on":true}}\n')

t0 = time.time()
total = 0
audio_bytes = 0
lines = 0
buf = b""
while time.time() - t0 < SEC:
    chunk = s.read(max(1, s.in_waiting or 1))
    if not chunk:
        continue
    total += len(chunk)
    buf += chunk
    while b"\n" in buf:
        line, buf = buf.split(b"\n", 1)
        if line.startswith(b'{"type":"wb_audio"'):
            audio_bytes += len(line)
            lines += 1

elapsed = time.time() - t0
s.write(b'{"type":"wb_debug_talk","payload":{"on":false}}\n')

# 排空
t1 = time.time()
drain = 0
while time.time() - t1 < 20:
    chunk = s.read(max(1, s.in_waiting or 1))
    if not chunk:
        if drain > 0 and time.time() - t1 > 1.5:
            break
        continue
    drain += len(chunk)
    t1 = time.time()
s.close()

print(f"录音 {elapsed:.1f}s：串口共收到 {total} 字节，其中音频 JSON {audio_bytes} 字节 / {lines} 条")
print(f"上行速率：{audio_bytes / elapsed / 1024:.1f} KB/s（音频 JSON）")
print(f"折合音频时长：{audio_bytes / (8000 / 4 * 4 / 3) / elapsed:.2f}× 实时" if audio_bytes else "")
print(f"结束时额外排空 {drain} 字节，用时 {time.time() - t1:.1f}s")
