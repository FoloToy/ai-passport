"""受控声学回环：PC 朗读固定句子 → 设备录音 → 识别，检查是否逐字正确。"""
import os
import sys
import threading
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "host"))

from devlink.server import _client

PHRASE = "帮我把这周的实验数据整理成表格"


def speak():
    import pythoncom
    import win32com.client as wc

    pythoncom.CoInitialize()
    v = wc.Dispatch("SAPI.SpVoice")
    v.Volume = 100
    v.Rate = -1
    v.Speak(PHRASE)


res = {}


def rec():
    res["r"] = _client({"op": "record", "sec": 7, "asr": True}, 90)


t = threading.Thread(target=rec)
t.start()
time.sleep(1.0)
speak()
t.join()

r = res.get("r") or {}
st = r.get("stats") or {}
print("原文:", PHRASE)
print(f"电平: rms={st.get('ac_rms')} peak={st.get('peak')} clean={st.get('clean_rms')}")
print("识别:", repr(r.get("text")))
got = (r.get("text") or "").replace("，", "").replace("。", "")
want = PHRASE.replace("，", "").replace("。", "")
print("完全一致:", got == want)
