"""下载 silero_vad.onnx（sherpa-onnx 的 VAD 模型，约 644KB）。"""
import os
import urllib.request

URL = ("https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/silero_vad.onnx")
DEST = r"C:\Users\46755\WorkBuddy\ai-passport\host\models\silero_vad.onnx"


def main():
    os.makedirs(os.path.dirname(DEST), exist_ok=True)
    if os.path.exists(DEST) and os.path.getsize(DEST) > 600000:
        print("已存在", os.path.getsize(DEST))
        return 0
    print("下载…", flush=True)
    with urllib.request.urlopen(URL, timeout=60) as r, open(DEST, "wb") as f:
        while True:
            b = r.read(65536)
            if not b:
                break
            f.write(b)
    print("完成", os.path.getsize(DEST))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
