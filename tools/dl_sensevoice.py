"""可靠的分块并行下载：每块独立校验长度、失败重试，最后按序拼装。

为什么不用"预分配 + 多线程写偏移"：任何一个 Range 请求失败（实测出现过 403），
那块就成了空洞，文件大小看起来对、模型却解析不了（Protobuf parsing failed）。
分块下载可以逐块确认字节数，缺哪块补哪块。
"""
import os
import sys
import time
import urllib.request
from concurrent.futures import ThreadPoolExecutor

BASE = ("https://hf-mirror.com/csukuangfj/"
        "sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/resolve/main/")
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   "host", "models", "sense-voice")
CHUNK = 4 * 1024 * 1024
WORKERS = 12


def total_size(url: str) -> int:
    req = urllib.request.Request(url, method="HEAD")
    with urllib.request.urlopen(req, timeout=30) as r:
        return int(r.headers.get("Content-Length") or 0)


def get_chunk(url: str, start: int, end: int, attempt: int = 0) -> bytes:
    want = end - start + 1
    try:
        req = urllib.request.Request(url, headers={"Range": f"bytes={start}-{end}"})
        with urllib.request.urlopen(req, timeout=120) as r:
            data = r.read()
        if len(data) != want:
            raise IOError(f"长度不符 {len(data)} != {want}")
        return data
    except Exception as e:
        if attempt >= 6:
            raise RuntimeError(f"分块 {start}-{end} 重试 6 次仍失败：{e}")
        time.sleep(0.8 * (attempt + 1))
        return get_chunk(url, start, end, attempt + 1)


def download(name: str) -> bool:
    url = BASE + name
    os.makedirs(OUT, exist_ok=True)
    dst = os.path.join(OUT, name)
    total = total_size(url)
    if not total:
        print(f"{name}: 拿不到大小", flush=True)
        return False
    if os.path.exists(dst) and os.path.getsize(dst) == total:
        print(f"{name}: 已存在且大小正确（{total/1048576:.1f} MB），跳过", flush=True)
        return True

    ranges = [(s, min(total - 1, s + CHUNK - 1)) for s in range(0, total, CHUNK)]
    print(f"{name}: {total/1048576:.1f} MB，{len(ranges)} 块，{WORKERS} 并发", flush=True)

    t0 = time.time()
    done = [0]
    parts: dict[int, bytes] = {}

    def work(i_s):
        i, (s, e) = i_s
        parts[i] = get_chunk(url, s, e)
        done[0] += len(parts[i])
        if done[0] % (CHUNK * 10) < CHUNK:
            print(f"  {done[0]/1048576:.0f}/{total/1048576:.0f} MB "
                  f"{time.time()-t0:.0f}s", flush=True)

    with ThreadPoolExecutor(max_workers=WORKERS) as ex:
        list(ex.map(work, enumerate(ranges)))

    with open(dst, "wb") as f:
        for i in range(len(ranges)):
            f.write(parts[i])

    got = os.path.getsize(dst)
    print(f"{name}: 完成 {got/1048576:.1f} MB / 期望 {total/1048576:.1f} MB，"
          f"{time.time()-t0:.0f}s", flush=True)
    return got == total


ok = True
for f in ("model.int8.onnx", "tokens.txt"):
    ok = download(f) and ok
print("ALL OK" if ok else "FAILED", flush=True)
sys.exit(0 if ok else 1)
