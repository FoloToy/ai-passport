#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
prep_cats.py —— cats/ 批量转换到 main/cat/ (薄壳, 重活在 png2lvgl.py)
================================================================

从 cats/<action_frame_dir>.png 批量调用 png2lvgl.py 生成 main/cat/cat_*.c/h。
png2lvgl.py 已统一处理: 抠背景 / 方向归一 (_l 自动镜像) / 水印透明空洞 / 方形裁切 / 缩放 / ARGB8888。

用法 (从项目根目录):
    python3 tools/prep_cats.py
    python3 tools/prep_cats.py --size 80
"""
import argparse
import glob
import os
import subprocess
import sys


def main():
    ap = argparse.ArgumentParser(description="批量调用 png2lvgl.py 转换 cats/ → main/")
    ap.add_argument("--src", default="cats", help="源 PNG 目录")
    ap.add_argument("--out", default="main/cat", help="输出 .c/.h 目录")
    ap.add_argument("--size", type=int, default=56, help="输出方形边长")
    ap.add_argument("--pad", type=float, default=0.04, help="bbox 外扩比例")
    ap.add_argument("--key-color", default="#F74859", help="AI 红底抠除色")
    ap.add_argument("--tolerance", type=int, default=50, help="抠色容差")
    ap.add_argument("--format", default="argb8888", choices=["argb8888", "rgb565", "i8"])
    args = ap.parse_args()

    tool = os.path.join(os.path.dirname(os.path.abspath(__file__)), "png2lvgl.py")
    if not os.path.isfile(tool):
        sys.exit(f"找不到 {tool}")

    files = sorted(glob.glob(os.path.join(args.src, "*.png")))
    if not files:
        sys.exit(f"{args.src}/ 下没有 PNG")

    print(f"输入: {args.src}/  ({len(files)} 张)")
    print(f"输出: {args.out}/     规格: {args.size}x{args.size} {args.format.upper()}\n")

    for f in files:
        cmd = [
                sys.executable, tool, f,
                "--key-color", args.key_color,
                "--tolerance", str(args.tolerance),
                "--size", str(args.size),
                "--pad", str(args.pad),
                "--format", args.format,
                "--out", args.out + "/",
            ]
        r = subprocess.run(cmd)
        if r.returncode != 0:
            sys.exit(f"转换失败: {f}")

    print(f"\n完成 {len(files)} 张.  C 文件: {args.out}/cat_*.c")


if __name__ == "__main__":
    main()