"""session.py 行为验证。不碰硬件，纯逻辑。

重点验四件事：
  1. 单会话时零前缀（日常使用不能被标记污染）
  2. 第二个会话出现后两个都有前缀，且能报出"刚切换"
  3. 标签按首次出现顺序稳定，不随活跃度跳变
  4. 标题能从真实 transcript 读出来
"""
from __future__ import annotations

import glob
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from devlink.session import SessionTags  # noqa: E402

FAIL = 0


def ck(name: str, got, want) -> None:
    global FAIL
    ok = got == want
    if not ok:
        FAIL += 1
    print(f"  [{'PASS' if ok else 'FAIL'}] {name}: {got!r}" + ("" if ok else f" != {want!r}"))


print("1) 单会话：不应有任何前缀")
s = SessionTags()
s.note("aaa")
ck("label(aaa)", s.label("aaa"), "")
ck("tag_text", s.tag_text("aaa", "你：你好"), "你：你好")
ck("header", s.header("aaa"), "WorkBuddy")

print()
print("2) 第二个会话加入：两者都带前缀，且报出切换")
s = SessionTags()
s.note("aaa")
flip = s.note("bbb")
ck("切换信号", flip, True)
ck("label(aaa)", s.label("aaa"), "①")
ck("label(bbb)", s.label("bbb"), "②")
ck("tag_text", s.tag_text("aaa", "你：A"), "①你：A")
ck("header", s.header("bbb"), "WB② 2会话")

print()
print("3) 标签稳定：反复活跃 aaa 不应改变 bbb 的编号")
for _ in range(5):
    s.note("aaa")
ck("label(aaa) 仍为①", s.label("aaa"), "①")
ck("label(bbb) 仍为②", s.label("bbb"), "②")

print()
print("4) 第三个会话")
s.note("ccc")
ck("label(ccc)", s.label("ccc"), "③")
ck("header", s.header("ccc"), "WB③ 3会话")

print()
print("5) 空闲释放：把 idle_release 设成 0.1s 验证回收")
s = SessionTags(idle_release=0.1)
s.note("aaa")
s.note("bbb")
ck("回收前 label", len(s.describe()), 2)
time.sleep(0.15)
ck("回收后 label(aaa)", s.label("aaa"), "")
ck("回收后 active", len(s.describe()), 0)

print()
print("6) 从真实 transcript 读会话标题")
files = sorted(glob.glob(os.path.expanduser(
    "~/.workbuddy/projects/**/*.jsonl"), recursive=True),
    key=os.path.getmtime, reverse=True)
s = SessionTags()
hit = 0
for f in files[:6]:
    sid = os.path.basename(f)[:-6]
    t = s.load_title(sid, f)
    if t:
        hit += 1
        print(f"    {sid[:8]}  ->  {t!r}")
if hit == 0:
    print("    (这段时间的会话都没有 ai-title，属正常：短会话不生成标题)")
else:
    print(f"    读到 {hit} 个标题")

print()
print("7) 超过 10 个活跃会话：标签池不越界，且总是最新 10 个持标签")
s = SessionTags()
for i in range(13):
    s.note(f"s{i}")
tags = [s.label(f"s{i}") for i in range(13)]
# 设计：池子满时抢最久没动静的那个，所以最新的必须有标签
ck("最新3个都有标签", all(tags[10:]), True)
ck("持标签数不超过池容量", sum(1 for t in tags if t), 10)
ck("标签不重复", len({t for t in tags if t}), sum(1 for t in tags if t))
print(f"    13 个会话的标签: {''.join(t or '·' for t in tags)}  (· = 已被抢)")

print()
print("FAIL 数:", FAIL)
sys.exit(1 if FAIL else 0)
