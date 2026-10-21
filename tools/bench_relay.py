"""公网链路可行性压测：把「串口」换成「TCP 中继」，看现有协议还成不成立。

不碰硬件、不碰 WorkBuddy。三件事：

  1. 协议是否零改动 —— 上行 JSON 走 TCP 透传后能否原样解析
  2. 带宽是否够     —— 语音上行实测 ~11.4 KB/s，中继能不能持续扛住
  3. 延迟是否致命   —— 播报是「领先 1.2s 推、固件缓冲 2.0s」的节拍流，
                       公网 RTT 会吃掉多少余量

结论只看三个数字：丢行数、上行吞吐、下行到达间隔的抖动。
"""
from __future__ import annotations

import asyncio
import base64
import os
import statistics
import sys
import time

HOST_DIR = r"C:\Users\46755\WorkBuddy\ai-passport\host"
sys.path.insert(0, HOST_DIR)
from devlink import proto  # noqa: E402

AUDIO_KBPS = 11.4          # 实测上行带宽
BLOCK_BYTES = 1536         # 128ms 一块（8k/16bit，2:1 ADPCM 后约 1.5KB）
TTS_BLOCK_ADPCM = 512      # terminal.py 的 TTS_CHUNK_ADPCM
TTS_BLOCK_MS = 128


async def relay(port: int, delay: float, ready: asyncio.Event):
    """极简中继：等两个连接，配对后双向透传；每段单向注入 delay 秒。"""

    async def pipe(reader, writer, tag):
        try:
            while True:
                data = await reader.read(65536)
                if not data:
                    break
                if delay:
                    await asyncio.sleep(delay)
                writer.write(data)
                await writer.drain()
        except (ConnectionError, asyncio.CancelledError):
            pass
        finally:
            try:
                writer.close()
            except Exception:
                pass

    async def on_conn(reader, writer):
        on_conn.pending.append((reader, writer))
        if len(on_conn.pending) >= 2:
            (r1, w1), (r2, w2) = on_conn.pending[0], on_conn.pending[1]
            on_conn.pending.clear()
            ready.set()
            await asyncio.gather(
                pipe(r1, w2, "a"), pipe(r2, w1, "b"), return_exceptions=True
            )

    on_conn.pending = []
    srv = await asyncio.start_server(on_conn, "127.0.0.1", port)
    async with srv:
        await srv.serve_forever()


async def run_case(delay: float, tag: str) -> dict:
    port = 38900 + int(delay * 1000) % 90
    ready = asyncio.Event()
    relay_task = asyncio.create_task(relay(port, delay, ready))

    await asyncio.sleep(0.15)
    dev_r, dev_w = await asyncio.open_connection("127.0.0.1", port)
    pc_r, pc_w = await asyncio.open_connection("127.0.0.1", port)
    await asyncio.wait_for(ready.wait(), 3)

    # ---------- 上行：设备模拟器按实时节拍灌语音 ----------
    sent_blocks = 40
    payload = base64.b64encode(os.urandom(BLOCK_BYTES * 3 // 4)).decode()

    async def device_tx():
        t0 = time.time()
        for seq in range(sent_blocks):
            line = proto.encode(proto.DEV_AUDIO, s=seq, b=payload)
            dev_w.write(line)
            await dev_w.drain()
            target = t0 + (seq + 1) * BLOCK_BYTES / (AUDIO_KBPS * 1024)
            gap = target - time.time()
            if gap > 0:
                await asyncio.sleep(gap)

    # ---------- 下行：PC 模拟器按 128ms 节拍推播报 ----------
    tts_blocks = 40

    async def pc_tx():
        t0 = time.time()
        for seq in range(tts_blocks):
            pc_w.write(proto.encode(proto.HOST_TTS_CHUNK, s=seq, b="AAAA"))
            await pc_w.drain()
            target = t0 + (seq + 1) * TTS_BLOCK_MS / 1000
            gap = target - time.time()
            if gap > 0:
                await asyncio.sleep(gap)

    rx = {"up": [], "down": [], "up_bytes": 0}
    rx_t0 = {"up": None, "down": None}

    async def reader_loop(reader, kind):
        buf = b""
        while True:
            try:
                chunk = await asyncio.wait_for(reader.read(65536), timeout=4.0)
            except asyncio.TimeoutError:
                return
            if not chunk:
                return
            if rx_t0[kind] is None:
                rx_t0[kind] = time.time()
            buf += chunk
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                msg = proto.parse(raw)
                if msg:
                    rx[kind].append(time.time())
                    if kind == "up":
                        rx["up_bytes"] += len(raw)

    tasks = [
        asyncio.create_task(device_tx()),
        asyncio.create_task(pc_tx()),
        asyncio.create_task(reader_loop(pc_r, "up")),
        asyncio.create_task(reader_loop(dev_r, "down")),
    ]
    await asyncio.gather(*tasks, return_exceptions=True)
    relay_task.cancel()
    for w in (dev_w, pc_w):
        try:
            w.close()
        except Exception:
            pass

    def gaps(ts):
        if len(ts) < 3:
            return []
        return [round((ts[i + 1] - ts[i]) * 1000, 1) for i in range(len(ts) - 1)]

    up_gaps = gaps(rx["up"])
    down_gaps = gaps(rx["down"])
    span = (rx["up"][-1] - rx["up"][0]) if len(rx["up"]) > 1 else 0
    kbps = rx["up_bytes"] / span / 1024 if span > 0 else 0

    return {
        "tag": tag,
        "上行收到": f"{len(rx['up'])}/{sent_blocks}",
        "下行收到": f"{len(rx['down'])}/{tts_blocks}",
        "上行吞吐KB/s": round(kbps, 1),
        "上行间隔ms": f"{min(up_gaps):.0f}/{statistics.median(up_gaps):.0f}/{max(up_gaps):.0f}" if up_gaps else "-",
        "下行间隔ms": f"{min(down_gaps):.0f}/{statistics.median(down_gaps):.0f}/{max(down_gaps):.0f}" if down_gaps else "-",
    }


async def main():
    print("协议层：换行分隔 JSON，无序号无确认 → 只要是有序字节流就能原样跑")
    print(f"上行模拟 {AUDIO_KBPS} KB/s 共 40 块；下行按 {TTS_BLOCK_MS}ms/块 共 40 块")
    print()
    rows = []
    for d, tag in [(0.0, "本地串口级 0ms"), (0.05, "同城 100ms RTT"),
                   (0.125, "跨省 250ms RTT"), (0.3, "劣化 600ms RTT")]:
        try:
            rows.append(await run_case(d, tag))
        except Exception as e:
            rows.append({"tag": tag, "结果": f"异常 {type(e).__name__}: {e}"})
        await asyncio.sleep(0.3)

    keys = ["tag", "上行收到", "下行收到", "上行吞吐KB/s", "上行间隔ms", "下行间隔ms"]
    widths = {k: max(len(k), *(len(str(r.get(k, ""))) for r in rows)) for k in keys}
    print("  ".join(k.ljust(widths[k]) for k in keys))
    print("  ".join("-" * widths[k] for k in keys))
    for r in rows:
        print("  ".join(str(r.get(k, "")).ljust(widths[k]) for k in keys))

    print()
    print("判据：")
    print("  · 上行间隔最坏值 < 500ms   → 录音不卡（设备侧 128ms 块，容 3 块滞后）")
    print("  · 下行间隔最坏值 < 800ms   → 播报不断（固件缓冲 2.0s，PC 只领先 1.2s）")
    print("  · 收发块数全等于 40        → 无丢行，协议可原样复用")
    return 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
