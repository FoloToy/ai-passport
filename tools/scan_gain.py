"""重新扫增益。固件已修（s_in_gain 会保持），所以这次结果才是真的。

上一轮扫描时固件每次 open codec 都把增益复位成 15dB，
所以我"设 21dB / 30dB"实际从没生效，得到的"随机跳"是假象。
"""
import sys
import threading
import time

sys.path.insert(0, r"C:\Users\46755\WorkBuddy\ai-passport\host")

from devlink.server import _client  # noqa: E402

PHRASE = "帮我把这周的实验数据整理成表格"


def main():
    import pythoncom
    import win32com.client as wc

    pythoncom.CoInitialize()
    v = wc.Dispatch("SAPI.SpVoice")
    v.Volume = 100

    print(f"原文: {PHRASE}", flush=True)
    print(f"{'增益':>6} {'rms':>7} {'peak':>7}  识别", flush=True)

    best = []
    for db in (15, 18, 21, 24, 27, 30):
        _client({"op": "gain", "db": db}, 8)
        time.sleep(0.6)

        res = {}

        def rec():
            res["r"] = _client({"op": "record", "sec": 6, "asr": True}, 60)

        t = threading.Thread(target=rec)
        t.start()
        time.sleep(0.9)
        v.Speak(PHRASE)
        t.join()

        r = res.get("r", {})
        st = r.get("stats", {})
        txt = r.get("text") or ""
        rms = st.get("ac_rms")
        peak = st.get("peak")
        hit = sum(1 for c in PHRASE if c in txt) / len(PHRASE)
        print(f"{db:5d}dB {rms:>7} {peak:>7}  {txt!r} ({hit:.0%})", flush=True)
        best.append((hit, db, rms, peak, txt))
        time.sleep(0.4)

    best.sort(reverse=True)
    print(f"\n最佳: {best[0][1]}dB  重合度 {best[0][0]:.0%}  rms={best[0][2]}", flush=True)
    _client({"op": "gain", "db": best[0][1]}, 8)
    print(f"已设为 {best[0][1]}dB", flush=True)


if __name__ == "__main__":
    main()
