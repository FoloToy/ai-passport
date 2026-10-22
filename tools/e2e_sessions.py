"""多会话标签的端到端验证（会真的推到设备屏幕）。

模拟两个不同 session_id 交替发消息，检查：
  1. 只有 1 个会话时，屏幕上不带任何前缀
  2. 第 2 个会话出现时，屏幕被清空并提示分区
  3. 两个会话的消息各自带 ①②
  4. cli sessions 能列出标签与会话标题

最后清屏，不在设备上留垃圾。
"""
from __future__ import annotations

import sys
import time

sys.path.insert(0, r"C:\Users\46755\WorkBuddy\ai-passport\host")
from devlink.server import _client  # noqa: E402

SID_A = "testA-0000-0000-0000-aaaaaaaaaaaa"
SID_B = "testB-0000-0000-0000-bbbbbbbbbbbb"


def send(req, wait=True):
    r = _client(req, 8)
    print(f"    -> {r}")
    time.sleep(1.2)
    return r


print("0) 复位标签表")
send({"op": "forget"})
send({"op": "clear"})

print()
print("1) 只有会话 A：应无前缀")
send({"op": "status", "state": "idle", "text": "单会话-测A", "session_id": SID_A})
send({"op": "reply", "text": "你：单会话你好", "session_id": SID_A})
print("    设备上应为『就绪/单会话-测A』与『WB 单会话你好』，无 ①")

print()
print("2) 会话 B 首次出现：应触发清屏 + 分区提示")
send({"op": "status", "state": "thinking", "text": "第二个来了", "session_id": SID_B})
print("    设备上应先被清空，出现『②新会话加入，屏幕已分区』")

print()
print("3) 两个会话交替：各自带前缀")
send({"op": "reply", "text": "你：A说话", "session_id": SID_A})
send({"op": "reply", "text": "你：B说话", "session_id": SID_B})
send({"op": "reply", "text": "你：A又说", "session_id": SID_A})
print("    设备上应为『①你：A说话』『②你：B说话』『①你：A又说』")

print()
print("4) cli sessions 查询")
r = _client({"op": "sessions"}, 5)
for x in r.get("sessions") or []:
    print(f"    {x['tag']}  {x['sid']}  idle={x['idle']}s  {x['title']}")

print()
print("5) 收尾：清屏 + 复位，不在设备留测试内容")
send({"op": "forget"})
send({"op": "clear"})
send({"op": "status", "state": "idle", "text": "多会话测试完成"})
print()
print("完成。请确认设备屏幕变化是否符合 1-3 的描述。")
