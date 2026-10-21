"""从链路末端取证：直接开串口发指令，读固件自己打的日志。

## 为什么不用服务

服务返回 `{"ok": true}` 只证明「指令从 socket 进来了、写进串口缓冲了」。
本项目已经四次栽在「返回值成功但副作用没发生」上（SendInput 少 8 字节、
PostMessage 返回 1 字没进去、SetValue 改了 DOM React 不认、hook 在 connect
就 exit）。所以这里绕开服务，直接对串口说话，然后**读固件吐回来的日志**：

    "标题栏 → WB② 2会话"     ← 证明 on_header 真被调用了
    "回复入库 who=② body_len=10"  ← 证明标签解析生效了

固件日志和协议 JSON 同走 USB-CDC，非 JSON 行本来会被 proto.parse 丢掉，
这里反过来把它们当成证据收起来。
"""
from __future__ import annotations

import sys
import time

import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM4"


def send(ser, obj_json: str) -> None:
    ser.write((obj_json + "\n").encode("utf-8"))
    ser.flush()


def main() -> int:
    print(f"打开 {PORT}（独占，先确认服务已停）")
    try:
        ser = serial.Serial(PORT, 115200, timeout=0.2)
    except serial.SerialException as e:
        print(f"打不开串口：{e}")
        print("→ 先停掉 devlink.server，否则串口被占用。")
        return 2

    def drain(seconds: float) -> list[str]:
        """收 seconds 秒，返回固件日志行（非 JSON）。"""
        end = time.time() + seconds
        buf = b""
        logs = []
        while time.time() < end:
            n = ser.in_waiting
            if n:
                buf += ser.read(n)
            if b"\n" in buf:
                while b"\n" in buf:
                    raw, buf = buf.split(b"\n", 1)
                    line = raw.decode("utf-8", "replace").strip()
                    if line and not line.startswith("{"):
                        logs.append(line)
            else:
                time.sleep(0.05)
        return logs

    print("先吃掉开机残留…")
    drain(1.5)

    tests = [
        ("单会话状态", '{"type":"wb_status","payload":{"state":"idle","text":"我回来了"}}'),
        ("单会话回复", '{"type":"wb_reply","payload":{"text":"你：单会话测试"}}'),
        ("多会话回复①", '{"type":"wb_reply","payload":{"text":"①你：A会话说话"}}'),
        ("多会话回复②", '{"type":"wb_reply","payload":{"text":"②你：B会话说话"}}'),
        ("标题栏更新", '{"type":"wb_header","payload":{"text":"WB② 2会话"}}'),
        ("心跳", '{"type":"wb_heartbeat","payload":{}}'),
    ]

    print()
    ok = 0
    for name, payload in tests:
        send(ser, payload)
        logs = drain(1.2)
        # 只留跟本次相关的日志
        hit = [l for l in logs if "标题栏" in l or "回复入库" in l]
        mark = "✓" if hit else "·"
        if hit:
            ok += 1
        print(f"[{mark}] {name}")
        if hit:
            for l in hit:
                print(f"      {l}")
        elif logs:
            for l in logs[-2:]:
                print(f"      (无相关日志) {l[:100]}")
        else:
            print("      (固件无输出)")

    print()
    print("再等一下，看有没有崩溃/看门狗复位")
    tail = drain(2.0)
    crash = [l for l in tail if any(k in l for k in
             ("Guru Meditation", "Backtrace", "abort()", "assert", "rst:", "boot:"))]
    if crash:
        print("!! 发现异常：")
        for l in crash[:8]:
            print("   ", l[:120])
    else:
        print("无崩溃迹象。")

    ser.close()
    print()
    print(f"有效证据 {ok}/{len(tests)} 条")
    return 0 if ok >= 4 and not crash else 1


if __name__ == "__main__":
    sys.exit(main())
