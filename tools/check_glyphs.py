#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_glyphs.py —— 校验固件里用到的每个汉字，LVGL 内置字库是否真的包含。

LVGL 遇到缺字会画占位方块（LV_USE_FONT_PLACEHOLDER=y），
所以"能编译、能跑、屏幕上一排方框"是完全可能的。这个脚本把这件事变成可验证的。
"""
import os
import re
import sys

PROJ = r"C:\Users\46755\WorkBuddy\ai-passport"
FONT = os.path.join(PROJ, "managed_components", "lvgl__lvgl", "src", "font",
                    "lv_font_source_han_sans_sc_16_cjk.c")


def parse_cmap(path):
    """从 LVGL 生成的字体 C 文件里还原出覆盖的码点集合。"""
    src = open(path, encoding="utf-8", errors="replace").read()

    # 1) 收集所有 unicode_list_N 数组（值相对 range_start 的偏移）
    lists = {}
    for m in re.finditer(r"static\s+const\s+uint16_t\s+(unicode_list_\d+)\s*\[\s*\]\s*=\s*\{(.*?)\};",
                         src, re.S):
        name, body = m.group(1), m.group(2)
        vals = [int(x, 0) for x in re.findall(r"0x[0-9a-fA-F]+|\d+", body)]
        lists[name] = vals

    # 2) 收集 cmaps[] 里每个条目的 range_start / range_length / unicode_list
    covered = set()
    for m in re.finditer(r"\{\s*\.range_start\s*=\s*(0x[0-9a-fA-F]+|\d+)\s*,"
                         r"\s*\.range_length\s*=\s*(0x[0-9a-fA-F]+|\d+)\s*,"
                         r".*?\.unicode_list\s*=\s*(unicode_list_\d+|NULL)\s*,",
                         src, re.S):
        start = int(m.group(1), 0)
        length = int(m.group(2), 0)
        lst = m.group(3)
        if lst == "NULL":
            # 连续区间
            covered.update(range(start, start + length))
        else:
            for off in lists.get(lst, []):
                covered.add(start + off)
    return covered


def cjk_chars(text):
    return {ch for ch in text if ord(ch) > 0x7F}


def main():
    if not os.path.exists(FONT):
        print("找不到字体文件:", FONT)
        return 1
    covered = parse_cmap(FONT)
    print(f"字库覆盖码点：{len(covered)} 个")

    # 扫描固件源码里的字符串字面量
    used = {}
    for name in ("workbuddy_companion.c",):
        path = os.path.join(PROJ, "main", name)
        src = open(path, encoding="utf-8", errors="replace").read()
        # 去掉注释，避免把说明文字算进去
        src = re.sub(r"//[^\n]*", "", src)
        src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
        for lit in re.findall(r'"((?:[^"\\]|\\.)*)"', src):
            for ch in cjk_chars(lit):
                used.setdefault(ch, set()).add(lit[:40])

    missing = {ch: lits for ch, lits in used.items() if ord(ch) not in covered}
    print(f"固件里用到的非 ASCII 字符：{len(used)} 个")

    if not missing:
        print("✅ 全部有字形")
    else:
        print(f"❌ 缺 {len(missing)} 个字形，屏幕上会显示成方框：")
        for ch, lits in sorted(missing.items()):
            print(f"   {ch!r} U+{ord(ch):04X}  出现在: {sorted(lits)[:2]}")

    # 顺便把常用界面词全查一遍，防止以后加字踩坑
    probe = "就绪思考中有提醒待确认聆听发秒没听清已给已发给正在处理这一轮做完了回来看一眼会话开始需要授权在等空闲会儿子任务完成允许拒绝确认执行命令读取写入网页搜索文件路径角色用户消息通知播报"
    miss2 = [ch for ch in cjk_chars(probe) if ord(ch) not in covered]
    print(f"常用界面词体检：{'✅ 全有' if not miss2 else '❌ 缺 ' + ''.join(sorted(set(miss2)))}")
    return 0 if not missing else 2


if __name__ == "__main__":
    sys.exit(main())
