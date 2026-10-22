"""校验 LVGL 稀疏字库到底覆盖了哪些码点。

为什么需要这个：LVGL 生成的 .c 字库用「tier + 相对偏移」的稀疏 cmap，
肉眼看到 `--symbols` 参数里有某字符，**不代表**它真的进了 cmap
（本项目就被 LVGL 内置字库骗过一次：名字像覆盖常用字，实际只有 1456 码点）。

解析顺序：
  1. 抽出所有 unicode_list_N 数组 → {N: [相对偏移...]}
  2. 抽出所有 cmap tier 定义（range_start / range_length / list_length / unicode_list）
  3. 逐 tier 还原真实码点，组装成全量覆盖集合
  4. 拿待测字符反查

用法:
    python check_font_cov.py                 # 默认查一批候选符号
    python check_font_cov.py ①②③ 会话
"""
from __future__ import annotations

import re
import sys

FONT = r"C:\Users\46755\WorkBuddy\ai-passport\main\fonts\lv_font_wb_zh_16.c"


def load_tiers(path: str = FONT) -> tuple[set[int], list[dict]]:
    src = open(path, encoding="utf-8", errors="replace").read()

    lists: dict[str, list[int]] = {}
    for m in re.finditer(
        r"static\s+const\s+uint16_t\s+(unicode_list_\d+)\[\]\s*=\s*\{(.*?)\};",
        src,
        re.S,
    ):
        vals = []
        for tok in m.group(2).replace("\n", " ").split(","):
            tok = tok.strip()
            if not tok:
                continue
            try:
                vals.append(int(tok, 0))
            except ValueError:
                pass
        lists[m.group(1)] = vals

    tiers = []
    for m in re.finditer(
        r"\.range_start\s*=\s*(0x[0-9a-fA-F]+|\d+)\s*,\s*"
        r"\.range_length\s*=\s*(0x[0-9a-fA-F]+|\d+)\s*,\s*"
        r"\.glyph_id_start\s*=\s*(\d+)\s*,\s*"
        r"\.unicode_list\s*=\s*(\w+)\s*,\s*"
        r"\.glyph_id_ofs_list\s*=\s*(\w+)\s*,\s*"
        r"\.list_length\s*=\s*(\d+)\s*,\s*"
        r"\.type\s*=\s*(LV_FONT_FMT_TXT_CMAP_\w+)",
        src,
    ):
        start = int(m.group(1), 0)
        length = int(m.group(2), 0)
        ulist = m.group(4)
        ln = int(m.group(6))
        fmt = m.group(7)
        offs = lists.get(ulist, [])
        tiers.append(
            {
                "range_start": start,
                "range_length": length,
                "list_length": ln,
                "format": fmt,
                "offsets": offs[:ln],
            }
        )

    covered: set[int] = set()
    for t in tiers:
        base = t["range_start"]
        if "SPARSE" in t["format"]:
            # 稀疏：unicode_list 存的是相对 range_start 的偏移，只这些码点存在
            for off in t["offsets"]:
                covered.add(base + off)
        else:
            # FORMAT0：整段连续
            covered.update(range(base, base + t["range_length"]))
    return covered, tiers


def main(argv: list[str]) -> int:
    covered, tiers = load_tiers()
    print(f"字库文件: {FONT}")
    print(f"cmap tier 数: {len(tiers)}")
    print(f"实际覆盖码点: {len(covered)}")
    print()

    if len(argv) > 1:
        chars = "".join(argv[1:])
    else:
        chars = "①②③④⑤⑥⑦⑧⑨⑩·—…↑↓←→✓会话项目资产梳理正在处理等待完成"

    missing = []
    for ch in dict.fromkeys(chars):
        ok = ord(ch) in covered
        print(f"  {ch!r}  U+{ord(ch):04X}  {'有' if ok else '缺'}")
        if not ok:
            missing.append(ch)

    print()
    if missing:
        print(f"缺字 {len(missing)} 个: {''.join(missing)}")
        print("→ 这些字符在固件上会渲染成方框。文案里必须避开。")
        return 1
    print("全部覆盖，可安全用于界面文案。")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
