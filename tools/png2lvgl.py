#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
png2lvgl.py —— cats/*.png → LVGL 9 C 数组 (单一完整转换工具)
============================================================

支持本次重构需求:
  1) 文件名命名 action_frame_dir.png (例 walk_3_l.png)
     - dir == 'l' 自动水平镜像, 归一化为朝右
     - dir == 'r' / 'right' / 缺省 → 原样 (朝右)
  2) 转换期内完成: 抠背景 + 镜像 + 方形裁切 + 缩放 + 像素格式
  3) AI 水印删除遗留的"空像素" (透明) 区域:
       - alpha>0 bbox 自动忽略透明空洞, 不会把水印孔误判为猫
       - 透明保持透明, 不填成黑/白块 (del 生成仍为 0)
  4) 默认符号名: cat_{action}_{frame} (去掉方向后缀, 朝右归一)

用法 (单张):
  python3 tools/png2lvgl.py cats/walk_3_l.png \
      --key-color "#F74859" --tolerance 50 --size 56 \
      --pad 0.04 --format argb8888 --out main/cat

  # 批量: 见 tools/prep_cats.py
"""
import argparse
import os
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("缺少 Pillow: pip3 install pillow")


def parse_hex(s):
    s = s.lstrip("#")
    if len(s) != 6:
        sys.exit(f"颜色格式错误: {s}, 应为 #RRGGBB")
    return (int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16))


def parse_name(stem):
    """文件名 action_frame_dir.png → (sym_base, action, direction)

    sym_base 不含 cat_ 前缀, 且已剥离方向后缀, 用于作为 C 符号的稳定名。
    direction 取 'l'/'r' 用于决定是否镜像。
    """
    parts = stem.split('_')
    if not parts or not parts[0]:
        sys.exit(f"文件名 {stem!r} 无法解析 (至少要有一个动作名)")
    action = parts[0]
    if len(parts) >= 3 and parts[-1] in ('r', 'l', 'right', 'left'):
        direction = 'l' if parts[-1] in ('l', 'left') else 'r'
        frame = '_'.join(parts[1:-1]) or '1'
        sym = f"{action}_{frame}"
    elif len(parts) == 2:
        direction = 'r'
        sym = f"{action}_{parts[1]}"
    else:
        direction = 'r'
        sym = action
    return sym, action, direction


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
    """LVGL9 lv_color32_t 内存布局 = [b, g, r, a] (小端)"""
    data = []
    for row in pixels:
        for (r, g, b, a) in row:
            if key and a > 0 and max(abs(r - key[0]), abs(g - key[1]), abs(b - key[2])) <= tol:
                a = 0
            data += [b, g, r, a]
    return data


def gen_rgb565(pixels, w, h, key, tol):
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


def gen_i8(pixels, w, h, key, tol, max_colors):
    hist = {}
    for row in pixels:
        for (r, g, b, a) in row:
            if key and a > 0 and max(abs(r - key[0]), abs(g - key[1]), abs(b - key[2])) <= tol:
                a = 0
            if a > 0:
                hist[(r, g, b)] = hist.get((r, g, b), 0) + 1
    top = sorted(hist, key=hist.get, reverse=True)[:max_colors]
    pal = [(0, 0, 0)] + top  # index 0 = 透明 (强制)
    if len(pal) > 256:
        sys.exit("颜色数超过 256, 请用 --colors 限制")

    def nearest(r, g, b):
        # 从 index 1 开始: index 0 是透明, 否则近黑像素会被误映成透明
        best, best_d = 1, 1 << 30
        for i in range(1, len(pal)):
            pr, pg, pb = pal[i]
            d = (pr - r) ** 2 + (pg - g) ** 2 + (pb - b) ** 2
            if d < best_d:
                best, best_d = i, d
        return best

    idx = []
    for row in pixels:
        for (r, g, b, a) in row:
            if a == 0:
                idx.append(0)
            else:
                idx.append(nearest(r, g, b))

    pal_data = [0x00, 0x00, 0x00, 0x00]  # index 0 = 全透明 (a=0 强制)
    for (r, g, b) in pal[1:]:
        pal_data += [b, g, r, 0xFF]
    for _ in range(256 - len(pal)):
        pal_data += [0x00, 0x00, 0x00, 0x00]
    return pal_data + idx, len(idx), 256 * 4


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
    ap.add_argument("--format", choices=["argb8888", "rgb565", "i8"],
                    default="argb8888")
    ap.add_argument("--colors", type=int, default=256, help="i8 调色板最大色数")
    ap.add_argument("--out", default="main/cat", help="输出目录")
    ap.add_argument("--no-mirror", action="store_true",
                    help="强制不镜像 (覆盖文件名 _l 自动镜像)")
    args = ap.parse_args()

    stem = os.path.splitext(os.path.basename(args.input))[0]
    sym, action, direction = parse_name(stem)
    name = args.name or f"cat_{sym}"

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
    else:  # i8
        raw, _n, _ = gen_i8(pixels, args.size, args.size, key, args.tolerance, args.colors)
        write_dsc(out, name, "LV_COLOR_FORMAT_I8",
                  args.size, args.size, args.size, raw)


if __name__ == "__main__":
    main()