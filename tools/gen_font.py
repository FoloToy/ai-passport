#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""gen_font.py —— 用 lv_font_conv 从系统黑体生成 LVGL 中文点阵字库。

为什么要自己生成：LVGL 内置的 source_han_sans_sc_16_cjk 只覆盖 1456 个码点，
"确认/拒绝/说话/就绪"这些常用字都不在里面，屏幕上会画成方块。

字符集 = ASCII + GB2312 全集（6763 汉字 + 全角标点）+ 几个界面符号。
这样 WorkBuddy 回复里的中文也几乎不会缺字。

用法：python tools/gen_font.py
"""
import os
import subprocess
import sys

PROJ = r"C:\Users\46755\WorkBuddy\ai-passport"
NODE = r"C:\Users\46755\.workbuddy\binaries\node\versions\22.22.2-3\node.exe"
CONV = os.path.join(PROJ, "tools", "node_modules", "lv_font_conv", "lv_font_conv.js")
SRC_FONT = os.path.join(os.environ["WINDIR"], "Fonts", "simhei.ttf")   # 黑体，小字号清楚
OUT_DIR = os.path.join(PROJ, "main", "fonts")
OUT_C = os.path.join(OUT_DIR, "lv_font_wb_zh_16.c")
SIZE = 16
BPP = 4


def gb2312_chars():
    """枚举 GB2312 全部字符（含符号区、一级、二级汉字）。"""
    out = []
    for b1 in range(0xA1, 0xF8):
        for b2 in range(0xA1, 0xFF):
            try:
                ch = bytes([b1, b2]).decode("gb2312")
            except UnicodeDecodeError:
                continue
            out.append(ch)
    return out


def main():
    if not os.path.exists(CONV):
        print("缺 lv_font_conv，先执行：")
        print(r'  cd tools && npm install lv_font_conv@1.5.2')
        return 1
    if not os.path.exists(SRC_FONT):
        print("缺字体文件:", SRC_FONT)
        return 1

    ascii_chars = [chr(c) for c in range(0x20, 0x7F)]
    extras = list("…↑↓←→·—✓℃±×÷°《》「」【】")
    chars = ascii_chars + extras + gb2312_chars()

    # 去重保序
    seen = set()
    uniq = []
    for ch in chars:
        if ch not in seen:
            seen.add(ch)
            uniq.append(ch)
    symbols = "".join(uniq)
    print(f"字符集：{len(symbols)} 个（ASCII {len(ascii_chars)} + 符号 {len(extras)} + GB2312）")

    os.makedirs(OUT_DIR, exist_ok=True)
    cmd = [
        NODE, CONV,
        "--font", SRC_FONT,
        "--symbols", symbols,
        "--size", str(SIZE),
        "--bpp", str(BPP),
        "--format", "lvgl",
        "--no-compress",
        "--lv-include", "lvgl.h",
        # C 符号名由输出文件名推导：lv_font_wb_zh_16.c → lv_font_wb_zh_16
        "-o", OUT_C,
    ]
    env = os.environ.copy()
    env.pop("NODE_OPTIONS", None)
    r = subprocess.run(cmd, capture_output=True, text=True,
                       encoding="utf-8", errors="replace", env=env)
    if r.returncode != 0:
        print("生成失败 rc=", r.returncode)
        print((r.stdout or "")[-1500:])
        print((r.stderr or "")[-1500:])
        return 1
    print(f"✅ 生成 {OUT_C}  {os.path.getsize(OUT_C)/1048576:.2f} MB 源码")
    if r.stdout.strip():
        print(r.stdout.strip()[-600:])
    return 0


if __name__ == "__main__":
    sys.exit(main())
