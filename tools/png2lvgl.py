#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
png2lvgl.py —— 单张 PNG → LVGL 9 C 数组

功能: 抠背景(色键) + 方向归一(朝左自动镜像) + 方形裁切 + 缩放 + 输出像素格式。
命名约定: <action>_<frame>_<dir>[_<motion>].png
  - dir: r=朝右, l=朝左(自动水平镜像成朝右)
  - motion 必填, 只描述画面怎么动, 与动物动作无关:
    idle / moveforward / sprintforward / moveup / sprintup
  - 生成的 C 符号名: <prefix>_<action>_<frame> (默认 pet_)

用法:
  python3 tools/png2lvgl.py pets/walk_3_l_moveforward.png \
      --key-color "#F74859" --tolerance 50 --size 56 \
      --pad 0.04 --format rgb565 --out main/pet

  # 批量转换见 tools/prep_pet.py
"""
import argparse
import os
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("缺少 Pillow: pip3 install pillow")


# ---------- 运动学预设 (动物无关的纯物理描述, 行为唯一真相) ----------
# 动作名 (walk/jump/sleep/groom/sit/peck...) 只作帧分组与显示标签,
# 不携带任何行为语义; 行为完全由 PNG 名的 motion 字段决定。
# motion 用 {速度档 move/sprint} × {运动轴 forward/up} 描述画面怎么动:
#   idle          不动 (原地循环)
#   moveforward   水平慢移    sprintforward 水平快移
#   moveup        垂直小弧(小跳) sprintup    垂直大弧(跳)
MOTION_KEYS = {'idle', 'moveforward', 'sprintforward', 'moveup', 'sprintup'}

MOTION_PRESETS = {
    # motion        : (stationary, step_px, hop_max, frame_ms, hold_ms)
    'idle':          dict(stationary=True,  step_px=0, hop_max=0,  frame_ms=350, hold_ms=3000),
    'moveforward':   dict(stationary=False, step_px=1, hop_max=0,  frame_ms=130, hold_ms=0),
    'sprintforward': dict(stationary=False, step_px=3, hop_max=0,  frame_ms=90,  hold_ms=0),
    'moveup':        dict(stationary=False, step_px=1, hop_max=6,  frame_ms=120, hold_ms=0),
    'sprintup':      dict(stationary=False, step_px=2, hop_max=12, frame_ms=110, hold_ms=0),
}

DIR_KEYS = {'r', 'l', 'right', 'left'}


def parse_hex(s):
    s = s.lstrip("#")
    if len(s) != 6:
        sys.exit(f"颜色格式错误: {s}, 应为 #RRGGBB")
    return (int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16))


def parse_name(stem, prefix="pet"):
    """文件名 <action>_<frame>_<dir>[_<motion>].png
        → (sym, action, direction, motion, frame)

    - motion 必填 (idle/moveforward/sprintforward/moveup/sprintup), 缺则报错 —— 行为唯一真相
    - dir == 'l' 自动水平镜像为朝右; dir == 'r'/'right'/缺省 原样
    - C 符号名 = {prefix}_{action}_{frame} (剥离方向与 motion 后缀)
    """
    parts = stem.split('_')
    if not parts or not parts[0]:
        sys.exit(f"文件名 {stem!r} 无法解析 (至少要有一个动作名)")
    action = parts[0]
    motion = None
    direction = 'r'

    if parts[-1] in MOTION_KEYS:
        motion = parts[-1]
        tail = parts[1:-1]
        if tail and tail[-1] in DIR_KEYS:
            direction = 'l' if tail[-1] in ('l', 'left') else 'r'
            tail = tail[:-1]
        frame = '_'.join(tail) or '1'
    elif parts[-1] in DIR_KEYS:
        direction = 'l' if parts[-1] in ('l', 'left') else 'r'
        tail = parts[1:-1]
        frame = '_'.join(tail) or '1'
    else:
        tail = parts[1:]
        frame = '_'.join(tail) or '1'

    if motion is None:
        sys.exit(f"{stem!r}: 缺少 motion 字段 (必需, 例: {action}_{frame}_r_moveforward)")

    sym = f"{prefix}_{action}_{frame}"
    return sym, action, direction, motion, frame


def c_byte_array(name, data, per_line=12):
    lines = []
    for i in range(0, len(data), per_line):
        chunk = data[i:i + per_line]
        lines.append("    " + ",".join(f"0x{b:02X}" for b in chunk) + ",")
    return (f"LV_ATTRIBUTE_MEM_ALIGN\n"
            f"static const uint8_t {name}_map[] = {{\n"
            + "\n".join(lines) + "\n};\n")


def write_dsc(path, name, cf, w, h, stride, data):
    body = c_byte_array(name, data)
    body += f"""
const lv_image_dsc_t {name} = {{
    .header = {{
        .magic  = LV_IMAGE_HEADER_MAGIC,
        .cf     = {cf},
        .flags  = 0,
        .w      = {w},
        .h      = {h},
        .stride = {stride},
    }},
    .data_size = sizeof({name}_map),
    .data      = {name}_map,
}};
"""
    with open(path + ".c", "w") as f:
        f.write("/* 由 png2lvgl.py 自动生成 —— 勿手改 */\n")
        f.write('#include "lvgl.h"\n\n')
        f.write(body)

    with open(path + ".h", "w") as f:
        f.write("/* 由 png2lvgl.py 自动生成 —— 勿手改 */\n")
        f.write(f"#ifndef LV_IMAGE_{name.upper()}_H\n")
        f.write(f"#define LV_IMAGE_{name.upper()}_H\n\n")
        f.write('#include "lvgl.h"\n\n')
        f.write(f"LV_IMG_DECLARE({name});\n\n")
        f.write(f"/* {w}x{h} {cf}\n")
        f.write(f" * LVGL scale 单位: 256 = 100%%, 640 = 250%% (2.5x)\n")
        f.write(f" * 放大到目标像素宽 TW: lv_image_set_scale(img, TW * 256 / {w}) */\n")
        f.write("#endif\n")

    print(f"✓ {os.path.basename(path):20s}  ({w}x{h} {cf}, 数据 {len(data)} 字节)")


# ---------- 关键转换步骤 (均在"转换时"完成) ----------

def key_to_alpha(im, key, tol):
    """把接近 key 色的非透明像素 alpha 置 0 (抠背景)"""
    if key is None:
        return im
    px = im.load(); w, h = im.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a > 0 and max(abs(r - key[0]), abs(g - key[1]), abs(b - key[2])) <= tol:
                px[x, y] = (0, 0, 0, 0)
    return im


def mirror_h(im):
    return im.transpose(Image.FLIP_LEFT_RIGHT)


def find_bbox(im):
    """alpha>0 区域的 bbox; 自动忽略水印删除遗留的透明空洞, 不影响猫本体范围"""
    px = im.load(); w, h = im.size
    minx, miny, maxx, maxy = w, h, -1, -1
    found = False
    for y in range(h):
        for x in range(w):
            if px[x, y][3] > 0:
                if not found:
                    minx = maxx = x; miny = maxy = y; found = True
                if x < minx: minx = x
                if y < miny: miny = y
                if x > maxx: maxx = x
                if y > maxy: maxy = y
    return (minx, miny, maxx, maxy) if found else None


def square_crop(im, bbox, pad):
    """以 bbox 中心裁正方形, 边长 = max(bw,bh)*(1+pad)。
       越界部分用透明画布承接, 保证猫 100% 完整, 不被切。"""
    minx, miny, maxx, maxy = bbox
    bw = maxx - minx + 1
    bh = maxy - miny + 1
    side = max(1, int(max(bw, bh) * (1.0 + pad)))
    cx = (minx + maxx) // 2
    cy = (miny + maxy) // 2
    sx = cx - side // 2
    sy = cy - side // 2
    out = Image.new('RGBA', (side, side), (0, 0, 0, 0))
    src_x0, src_y0 = max(0, sx), max(0, sy)
    src_x1, src_y1 = min(im.size[0], sx + side), min(im.size[1], sy + side)
    if src_x1 > src_x0 and src_y1 > src_y0:
        region = im.crop((src_x0, src_y0, src_x1, src_y1))
        out.paste(region, (src_x0 - sx, src_y0 - sy))
    return out


# ---------- 像素格式生成 ----------

def gen_argb(pixels, w, h, key, tol):
    """LV_COLOR_FORMAT_ARGB8888: 4 字节/像素, 带完整 8 位 alpha。

    - 内存布局 = [b, g, r, a] (小端, 即 LVGL9 的 lv_color32_t)。
    - 质量最高、体积最大 (4B/px): 56x56 宠物一帧 12544 字节。
    - LVGL 原生支持、渲染最稳, 无任何解码器坑。
    - 缺点: 对 ESP32-C3 (无 PSRAM, 1MB app 分区) 偏占 flash;
      只在需要最高画质、且空间宽松时选用。
    """
    data = []
    for row in pixels:
        for (r, g, b, a) in row:
            if key and a > 0 and max(abs(r - key[0]), abs(g - key[1]), abs(b - key[2])) <= tol:
                a = 0
            data += [b, g, r, a]
    return data


def gen_rgb565(pixels, w, h, key, tol):
    """LV_COLOR_FORMAT_RGB565: 2 字节/像素, 不透明 (无 alpha 通道)。

    - 打包格式: 红 5bit + 绿 6bit + 蓝 5bit = 16bit/px, 小端 [低字节, 高字节]。
    - 注意: 没有 alpha, 被抠掉的透明像素只能填黑 (0x0000)
    - 体积最小 (2B/px): 240x320 背景约 150KB, 约为 ARGB8888 的一半。
    - 渲染稳定 (解码器原生支持), 适配 ESP32-C3 的 1MB app 分区。
    """
    data = []
    for row in pixels:
        for (r, g, b, a) in row:
            if key and a > 0 and max(abs(r - key[0]), abs(g - key[1]), abs(b - key[2])) <= tol:
                a = 0
            if a == 0:
                data += [0x00, 0x00]
            else:
                v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                data += [v & 0xFF, v >> 8]
    return data


def gen_rgb565a8(pixels, w, h, key, tol):
    """LV_COLOR_FORMAT_RGB565A8: 颜色(RGB565, 2B/px) + 独立 8bit alpha 块 (1B/px), 共 3B/px。

    - 带完整 8 位 alpha, 抠掉的背景不再变黑, 边缘平滑保留; 比 ARGB8888 省 25%。
    - 数据布局: [颜色块 w*2/行 * h] + [alpha 块 w/行 * h], dsc.stride = w*2,
      与 lv_draw_buf_width_to_stride(RGB565A8)=w*2 及解码器 alpha 偏移(stride*h)严格对齐。
    - 当前宠物的默认格式: 透明正确、体积适中 (56x56 一帧 9408B)、渲染有完整快路径。
    """
    color = []
    alpha = []
    for row in pixels:
        for (r, g, b, a) in row:
            if key and a > 0 and max(abs(r - key[0]), abs(g - key[1]), abs(b - key[2])) <= tol:
                a = 0
            if a == 0:
                color += [0x00, 0x00]
                alpha += [0x00]
            else:
                v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                color += [v & 0xFF, v >> 8]
                alpha += [a & 0xFF]
    return color + alpha


# ---------- 背景图 (整屏矩形, 不裁切, 默认 RGB565) ----------

def make_bg(path, out_dir, w, h, fmt="rgb565", key_color=None, tol=32, name="pet_bg"):
    """生成铺满整屏的背景图 (pets/background.png → pet_bg.c/.h)。

    与宠物不同: 不做 bbox / 方形裁切, 直接缩放铺满 w×h。
    默认 RGB565 (打包 16bit/px, 240x320 ≈ 150KB), 不需调色板、渲染稳定,
    体积约为 ARGB8888 (300KB) 的一半, 适配 ESP32-C3 1MB app 分区。
    """
    img = Image.open(path).convert("RGBA")
    key = parse_hex(key_color) if key_color else None
    if key:
        img = key_to_alpha(img, key, tol)
    img = img.resize((w, h), Image.NEAREST)
    px = img.load()
    pixels = [[px[x, y] for x in range(w)] for y in range(h)]
    out = os.path.join(out_dir, name)
    if fmt == "argb8888":
        data = gen_argb(pixels, w, h, key, tol)
        write_dsc(out, name, "LV_COLOR_FORMAT_ARGB8888", w, h, w * 4, data)
    elif fmt == "rgb565":
        data = gen_rgb565(pixels, w, h, key, tol)
        write_dsc(out, name, "LV_COLOR_FORMAT_RGB565", w, h, w * 2, data)
    else:
        sys.exit(f"不支持的背景格式: {fmt}")
    return name


# ---------- 入口 ----------

def main():
    ap = argparse.ArgumentParser(
        description="PNG → LVGL9 C 数组 (含方向归一 + 水印透明空洞处理)")
    ap.add_argument("input", help="输入 PNG (建议命名 action_frame_dir.png)")
    ap.add_argument("--name", default=None, help="C 符号名, 留空按文件名推导")
    ap.add_argument("--key-color", default=None, help="#RRGGBB 抠背景色")
    ap.add_argument("--tolerance", type=int, default=32, help="抠背景容差 0-255")
    ap.add_argument("--size", type=int, default=64, help="输出方形边长")
    ap.add_argument("--pad", type=float, default=0.04, help="bbox 外扩比例 (防裁切)")
    ap.add_argument("--format", choices=["argb8888", "rgb565", "rgb565a8"],
                    default="rgb565a8")
    ap.add_argument("--prefix", default="pet", help="C 符号前缀 (默认 pet → pet_walk_1)")
    ap.add_argument("--out", default="main/pet", help="输出目录")
    ap.add_argument("--no-mirror", action="store_true",
                    help="强制不镜像 (覆盖文件名 _l 自动镜像)")
    args = ap.parse_args()

    stem = os.path.splitext(os.path.basename(args.input))[0]
    sym, action, direction, motion, frame = parse_name(stem, args.prefix)
    name = args.name or sym

    # 1) 打开
    img = Image.open(args.input).convert("RGBA")

    # 2) 抠背景 (转换期内)
    key = parse_hex(args.key_color) if args.key_color else None
    img = key_to_alpha(img, key, args.tolerance)

    # 3) 方向归一: _l 自动镜像成朝右 (需求 1)
    if not args.no_mirror and direction == 'l':
        img = mirror_h(img)
        print(f"  ↺ {args.input} (dir=l → 已水平镜像为朝右)")

    # 4) bbox — alpha>0 自动排除水印透明空洞 (需求 2/3)
    bbox = find_bbox(img)
    if not bbox:
        sys.exit(f"{args.input}: 未找到猫 (alpha>0 区域为空)")

    # 5) 方形裁切 (透明画布承接, 猫 100% 不裁)
    sq = square_crop(img, bbox, args.pad)

    # 6) NEAREST 缩放到目标尺寸 (保留像素风硬边)
    sqN = sq.resize((args.size, args.size), Image.NEAREST)
    px = sqN.load()
    pixels = [[px[x, y] for x in range(args.size)] for y in range(args.size)]

    out = os.path.join(args.out, name)
    if args.format == "argb8888":
        data = gen_argb(pixels, args.size, args.size, key, args.tolerance)
        write_dsc(out, name, "LV_COLOR_FORMAT_ARGB8888",
                  args.size, args.size, args.size * 4, data)
    elif args.format == "rgb565":
        data = gen_rgb565(pixels, args.size, args.size, key, args.tolerance)
        write_dsc(out, name, "LV_COLOR_FORMAT_RGB565",
                  args.size, args.size, args.size * 2, data)
    elif args.format == "rgb565a8":
        data = gen_rgb565a8(pixels, args.size, args.size, key, args.tolerance)
        write_dsc(out, name, "LV_COLOR_FORMAT_RGB565A8",
                  args.size, args.size, args.size * 2, data)


if __name__ == "__main__":
    main()