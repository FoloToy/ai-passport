#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import glob
import os
import subprocess
import sys

# 复用 png2lvgl 的解析与运动学预设 (同一目录)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import png2lvgl  # noqa: E402


def motion_enum(motion):
    return {'idle': 'MOT_IDLE', 'moveforward': 'MOT_MOVEFORWARD',
            'sprintforward': 'MOT_SPRINTFORWARD', 'moveup': 'MOT_MOVEUP',
            'sprintup': 'MOT_SPRINTUP'}[motion]


def build_manifest(entries, out_dir, size, has_bg):
    """按 action 分组、帧号排序, 生成 pet_manifest.h (行为总表)。

    entries: list of (sym, action, direction, motion, frame)
    行为参数全部来自 png2lvgl.MOTION_PRESETS, 与动作名语义无关。
    """
    groups = {}  # action -> [(frame_int, sym, motion), ...]
    for (sym, action, _dir, motion, frame) in entries:
        try:
            fi = int(frame)
        except ValueError:
            fi = 0
        groups.setdefault(action, []).append((fi, sym, motion))

    actions = sorted(groups.keys())
    includes = []
    frame_arrays = []
    act_rows = []
    rest_pool = []
    move_pool = []

    for idx, action in enumerate(actions):
        items = sorted(groups[action], key=lambda x: x[0])
        syms = [s for (_, s, _) in items]
        motion = items[0][2]
        for (_, _, m) in items:
            if m != motion:
                sys.exit(f"动作 {action} 各帧 motion 不一致: {m} vs {motion}")

        preset = png2lvgl.MOTION_PRESETS[motion]
        for s in syms:
            includes.append(f'#include "{s}.h"')
        arr = (f'static const lv_image_dsc_t *const pet_frames_{action}[] = '
               f'{{ {", ".join("&" + s for s in syms)} }};')
        frame_arrays.append(arr)

        name = action.upper()
        act_rows.append(
            f'  /* {name} */ {{ "{name}", pet_frames_{action}, {len(syms)}, '
            f'{preset["frame_ms"]}, {preset["hold_ms"]}, '
            f'{motion_enum(motion)}, {preset["step_px"]}, {preset["hop_max"]}, '
            f'{str(preset["stationary"]).lower()} }},'
        )
        if preset['stationary']:
            rest_pool.append(idx)
        else:
            move_pool.append(idx)

    body = (
        "/* 由 prep_pet.py 自动生成 —— 勿手改 */\n"
        "#ifndef PET_MANIFEST_H\n"
        "#define PET_MANIFEST_H\n\n"
        "#include \"lvgl.h\"\n\n"
        "#define PET_HAS_BG  " + ("1" if has_bg else "0") + "\n"
        + ("#include \"pet_bg.h\"\n\n" if has_bg else "\n")
        + "\n".join(includes) + "\n\n"
        "#define PET_SPRITE_W  " + str(size) + "\n"
        "#define PET_SPRITE_H  " + str(size) + "\n\n"
        "typedef enum { MOT_IDLE, MOT_MOVEFORWARD, MOT_SPRINTFORWARD, MOT_MOVEUP, MOT_SPRINTUP } pet_mot_t;\n\n"
        "typedef struct {\n"
        "    const char *name;                        /* 显示标签 (动作名大写) */\n"
        "    const lv_image_dsc_t *const *frames;    /* 帧指针表 */\n"
        "    uint8_t  n_frames;\n"
        "    uint16_t frame_ms;     /* 换帧周期 (ms) */\n"
        "    uint16_t hold_ms;      /* 静止持续; 0 = 移动到撞墙 */\n"
        "    pet_mot_t motion;      /* 运动学类型 */\n"
        "    uint8_t  step_px;      /* 每 tick 水平位移 */\n"
        "    int8_t   hop_max;      /* 垂直跳跃幅度 (0 = 不跳) */\n"
        "    bool     stationary;   /* true = 原地不动 */\n"
        "} pet_action_t;\n\n"
        + "\n".join(frame_arrays) + "\n\n"
        "static const pet_action_t pet_actions[] = {\n"
        + "\n".join(act_rows) + "\n"
        "};\n\n"
        f"#define PET_ACT_COUNT  {len(actions)}\n\n"
        "/* 静止动作下标池 (撞墙后随机休息) */\n"
        f"static const uint8_t pet_rest_pool[] = {{ {', '.join(str(i) for i in rest_pool)} }};\n"
        f"#define PET_REST_N  (sizeof(pet_rest_pool) / sizeof(pet_rest_pool[0]))\n\n"
        "/* 移动动作下标池 (休息完恢复移动 / 移动中随机切换) */\n"
        f"static const uint8_t pet_move_pool[] = {{ {', '.join(str(i) for i in move_pool)} }};\n"
        f"#define PET_MOVE_N  (sizeof(pet_move_pool) / sizeof(pet_move_pool[0]))\n\n"
        "#endif /* PET_MANIFEST_H */\n"
    )

    path = os.path.join(out_dir, "pet_manifest.h")
    with open(path, "w") as f:
        f.write(body)
    print(f"✓ {os.path.basename(path):20s}  ({len(actions)} 动作, "
          f"{len(rest_pool)} 静止 / {len(move_pool)} 移动)")


def main():
    ap = argparse.ArgumentParser(
        description="批量调用 png2lvgl.py 转换 pets/ → main/pet/ 并生成 pet_manifest.h")
    ap.add_argument("--src", default="pets", help="源 PNG 目录")
    ap.add_argument("--out", default="main/pet", help="输出 .c/.h 目录")
    ap.add_argument("--prefix", default="pet", help="C 符号前缀 (默认 pet)")
    ap.add_argument("--size", type=int, default=56, help="输出方形边长")
    ap.add_argument("--pad", type=float, default=0.04, help="bbox 外扩比例")
    ap.add_argument("--key-color", default="#F74859", help="AI 红底抠除色")
    ap.add_argument("--tolerance", type=int, default=50, help="抠色容差")
    ap.add_argument("--format", default="rgb565a8", choices=["argb8888", "rgb565", "rgb565a8"])
    args = ap.parse_args()

    tool = os.path.join(os.path.dirname(os.path.abspath(__file__)), "png2lvgl.py")
    if not os.path.isfile(tool):
        sys.exit(f"找不到 {tool}")

    files = sorted(glob.glob(os.path.join(args.src, "*.png")))
    files = [f for f in files if os.path.splitext(os.path.basename(f))[0] != "background"]
    if not files:
        sys.exit(f"{args.src}/ 下没有宠物动作 PNG (background.png 不算动作)")

    print(f"输入: {args.src}/  ({len(files)} 张)")
    print(f"输出: {args.out}/     规格: {args.size}x{args.size} {args.format.upper()}\n")

    os.makedirs(args.out, exist_ok=True)

    entries = []
    for f in files:
        stem = os.path.splitext(os.path.basename(f))[0]
        # 先解析收集 (motion 缺失会直接报错退出)
        sym, action, direction, motion, frame = png2lvgl.parse_name(stem, args.prefix)
        entries.append((sym, action, direction, motion, frame))

        cmd = [
            sys.executable, tool, f,
            "--key-color", args.key_color,
            "--tolerance", str(args.tolerance),
            "--size", str(args.size),
            "--pad", str(args.pad),
            "--format", args.format,
            "--prefix", args.prefix,
            "--out", args.out + "/",
        ]
        r = subprocess.run(cmd)
        if r.returncode != 0:
            sys.exit(f"转换失败: {f}")

    os.makedirs(args.out, exist_ok=True)

    # 可选背景: pets/background.png → pet_bg (整屏 240x320, RGB565 稳定渲染)
    bg_src = os.path.join(args.src, "background.png")
    has_bg = os.path.isfile(bg_src)
    if has_bg:
        png2lvgl.make_bg(bg_src, args.out, 240, 320, "rgb565", args.key_color, args.tolerance)
        print(f"✓ background.png → pet_bg  (240x320 RGB565)")

    build_manifest(entries, args.out, args.size, has_bg)
    print(f"\n完成 {len(files)} 张动作"
          + (" (+ 背景图 pet_bg)" if has_bg else "")
          + f".  C 文件: {args.out}/pet_*.c  总表: {args.out}/pet_manifest.h")


if __name__ == "__main__":
    main()
