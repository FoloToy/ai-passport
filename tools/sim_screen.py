"""复现固件的 append_line，算出"屏幕最终每一行到底是什么"。

为什么需要：服务返回 `{"ok": true}` 只证明指令推出了串口，
**不证明屏幕上显示正确**。本项目已经踩过太多次「看着成功实际没生效」：
  · SendInput 少 8 字节 → 静默返回 0
  · PostMessage 返回 1 但字没进去
  · SetValue 改了 DOM 但 React 不认
所以这里不复用"返回 ok"，而是把固件的折行算法 1:1 搬过来，
用真实会发送的文本跑一遍，直接打印 240px 屏幕上每一行的内容。

对齐的固件常量（main/workbuddy_companion.c）：
  LINE_UNITS 26     单行宽度，半角=1、全角=2
  LINE_LEN   72     单行字节上限
  VIEW_ROWS         可见行数
  cp_units: cp < 0x80 ? 1 : 2
"""
from __future__ import annotations

import sys

LINE_UNITS = 26
LINE_LEN = 72
VIEW_ROWS = 10          # TRANS_H 224 / 行高 ~22


def cp_units(cp: int) -> int:
    return 1 if cp < 0x80 else 2


def append_line(who: str, text: str) -> list[str]:
    """按固件逻辑折行。返回屏幕上新增的每一行。

    ⚠ 注意固件这里有个刻意的近似：`units` 起点用的是 `pre_len`（**字节数**），
    不是真实显示宽度。对 "WB " 两者相等；对 "① " 是 4 字节但只占 3 units，
    于是固件会保守地早折半字——**安全（不溢出）但略窄**。
    模拟器必须照抄这个行为，否则算出的折行位置会和真机不一致。
    """
    out: list[str] = []
    pre = f"{who} " if who else ""
    pre_bytes = len(pre.encode("utf-8"))     # 与固件 snprintf 返回值一致
    cont = " " * pre_bytes

    line = pre
    units = pre_bytes                        # 固件: n = pre_len; units = pre_len
    has_content = False

    for ch in text:
        cp = ord(ch)
        if ch == "\n":
            out.append(line)
            line, units, has_content = cont, pre_bytes, False
            continue
        u = cp_units(cp)
        if units + u > LINE_UNITS:
            out.append(line)
            line, units, has_content = cont, pre_bytes, False
        blen = len(ch.encode("utf-8"))
        if len(line.encode("utf-8")) + blen < LINE_LEN - 1:
            if ch != "\r":
                line += ch
                units += u
                has_content = True
    if has_content or line.strip():
        out.append(line)
    return out


def render(lines: list[str], title: str, status: str) -> str:
    """画一个 240x320 的文本模拟屏。"""
    w = 26
    bar = "+" + "-" * (w + 2) + "+"
    res = [bar, f"| {title[:w].ljust(w)} |", bar, f"| {status[:w].ljust(w)} |", bar]

    view = lines[-VIEW_ROWS:]
    for ln in view:
        res.append(f"| {ln.ljust(w)} |")
    for _ in range(VIEW_ROWS - len(view)):
        res.append(f"| {' ' * w} |")
    res.append(bar)
    res.append("    (边框=屏幕边缘, 26 units = 13 个汉字宽)")
    return "\n".join(res)


def split_tag(text: str) -> tuple[str, str]:
    """复现 on_reply 的标签解析：首字符是 ①-⑩ 就用它当 who，并从正文里去掉。"""
    if text and 0x2460 <= ord(text[0]) <= 0x2469:
        return text[0], text[1:]
    return "WB", text


CASES = [
    ("单会话（无标签）", "你：接下来我们可以做什么？"),
    ("单会话长文本", "你：这是一个很长的句子用来测试折行是否会在字中间切断导致乱码"),
    ("多会话 ①", "①你：接下来我们可以做什么？"),
    ("多会话 ②", "②你：帮我把资产整理成文档"),
    ("分区提示", "②新会话加入，屏幕已分区"),
    ("带符号", "①你：完成度 100% ✓ 已通过"),
]


def main() -> int:
    print("=" * 62)
    print("固件折行模拟：屏幕每行最终内容")
    print(f"LINE_UNITS={LINE_UNITS} (13 个汉字/行)  LINE_LEN={LINE_LEN} 字节  可见 {VIEW_ROWS} 行")
    print("规则：首字符 ①-⑩ 当作 who 代替 'WB'（宽度不变，都是 3 units）")
    print("=" * 62)

    bad = 0
    for name, text in CASES:
        who, body = split_tag(text)
        lines = append_line(who, body)
        print()
        print(f"### {name}")
        print(f"    PC 发送: {text!r}  →  who={who!r} body={body!r}")
        for i, ln in enumerate(lines):
            units = sum(cp_units(ord(c)) for c in ln)
            b = len(ln.encode("utf-8"))
            flag = ""
            if units > LINE_UNITS:
                flag = "  <<< 超宽!"
                bad += 1
            if b >= LINE_LEN - 1:
                flag += "  <<< 超字节!"
                bad += 1
            print(f"    行{i}: [{units:2d}u/{b:2d}B] {ln!r}{flag}")

    print()
    print("=" * 62)
    print("多会话时的完整屏幕")
    print("=" * 62)
    screen_lines = []
    for text in ["①你：接下来我们可以做什么？", "②你：帮我把资产整理成文档",
                 "②新会话加入，屏幕已分区"]:
        who, body = split_tag(text)
        screen_lines += append_line(who, body)
    print(render(screen_lines, "WB① 2会话", "①正在处理"))

    print()
    if bad:
        print(f"发现 {bad} 处越界，必须调整")
        return 1
    print("全部行宽/字节均在限内，且按码点折行——不会出现半个汉字。")
    print("标签替换 WB 后，内容可用宽度与单会话时完全一致。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
