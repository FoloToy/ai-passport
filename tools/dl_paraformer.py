"""下载 sherpa-onnx paraformer 中文模型（多线程分段，可续传）。

GitHub releases 单连接约 0.3MB/s，8 段并行能快数倍。
"""
import concurrent.futures
import os
import tarfile
import time
import urllib.request

URL = ("https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/"
       "sherpa-onnx-paraformer-zh-small-2024-03-09.tar.bz2")
DEST_DIR = r"C:\Users\46755\WorkBuddy\ai-passport\host\models"
TAR = os.path.join(DEST_DIR, "paraformer-zh-small.tar.bz2")
TOTAL = 77920048
PARTS = 8
CHUNK = 1 << 20


def fetch(args):
    idx, start, end = args
    path = TAR + ".p%d" % idx
    have = os.path.getsize(path) if os.path.exists(path) else 0
    want = end - start + 1
    if have >= want:
        return idx, have
    for attempt in range(6):
        try:
            req = urllib.request.Request(URL, headers={"Range": "bytes=%d-%d" % (start + have, end)})
            with urllib.request.urlopen(req, timeout=60) as r, open(path, "ab") as f:
                while True:
                    b = r.read(CHUNK)
                    if not b:
                        break
                    f.write(b)
                    have = f.tell()
            if have >= want:
                return idx, have
        except Exception as e:
            print("  part%d 重试 %d: %s" % (idx, attempt, type(e).__name__), flush=True)
            time.sleep(2)
    return idx, have


def main():
    os.makedirs(DEST_DIR, exist_ok=True)
    if os.path.exists(TAR) and os.path.getsize(TAR) == TOTAL:
        print("已下载完整", TOTAL)
    else:
        if os.path.exists(TAR):
            os.remove(TAR)
        step = TOTAL // PARTS
        jobs = [(i, i * step, TOTAL - 1 if i == PARTS - 1 else (i + 1) * step - 1)
                for i in range(PARTS)]
        t0 = time.time()
        with concurrent.futures.ThreadPoolExecutor(max_workers=PARTS) as ex:
            for idx, got in ex.map(fetch, jobs):
                print("  part%d %d/%d" % (idx, got, jobs[idx][2] - jobs[idx][1] + 1), flush=True)
        with open(TAR, "wb") as out:
            for i in range(PARTS):
                p = TAR + ".p%d" % i
                with open(p, "rb") as f:
                    while True:
                        b = f.read(CHUNK)
                        if not b:
                            break
                        out.write(b)
                os.remove(p)
        print("合并完成 %d 字节，用时 %.0fs" % (os.path.getsize(TAR), time.time() - t0))

    if os.path.getsize(TAR) != TOTAL:
        print("大小不符，退出")
        return 1

    print("解包…")
    with tarfile.open(TAR, "r:bz2") as tf:
        tf.extractall(DEST_DIR)
    for name in os.listdir(DEST_DIR):
        p = os.path.join(DEST_DIR, name)
        if os.path.isdir(p) and "paraformer" in name:
            print("模型目录:", p)
            for f in os.listdir(p):
                print("   ", os.path.getsize(os.path.join(p, f)), f)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
