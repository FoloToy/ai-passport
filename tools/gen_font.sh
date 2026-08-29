#!/usr/bin/env bash
# tools/gen_font.sh —— 重新生成 main/font_sm_cjk_16.c（创智 Passport 中文 UI 字库）。
#
# 用法:
#   ./tools/gen_font.sh [NotoSansCJKsc-Regular.otf 路径]
#
# 依赖: node + npm（临时安装 lv_font_conv 到 /tmp/sm-font-tool）。
# 字符集: 自动从 main/passport_ui.c 的字符串字面量提取 CJK 字符 + ASCII 可打印区。
# 字体源: Noto Sans CJK SC Regular（SIL OFL 1.1），见 assets/fonts/README.md。
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
font_path="${1:-/tmp/NotoSansCJKsc-Regular.otf}"
tool_dir="/tmp/sm-font-tool"
out_file="${repo_root}/main/font_sm_cjk_16.c"

if [[ ! -f "${font_path}" ]]; then
    echo "Font not found: ${font_path}" >&2
    echo "Download: curl -sSL -o ${font_path} \\" >&2
    echo "  https://github.com/notofonts/noto-cjk/raw/main/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf" >&2
    exit 1
fi

if [[ ! -x "${tool_dir}/node_modules/.bin/lv_font_conv" ]]; then
    mkdir -p "${tool_dir}"
    (cd "${tool_dir}" && npm init -y >/dev/null && npm install lv_font_conv)
fi

chars="$(python3 - <<'EOF'
import re
text = open("main/passport_ui.c", encoding="utf-8").read()
text = re.sub(r"//[^\n]*", "", text)
text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
lits = re.findall(r'"((?:[^"\\]|\\.)*)"', text)
chars = set()
for lit in lits:
    chars.update(re.findall(r"[　-鿿＀-￯—（）]", lit))
print("".join(sorted(chars)), end="")
EOF
)"

cd "${repo_root}"
echo "Subsetting ${#chars} CJK glyphs + ASCII printable range..."
"${tool_dir}/node_modules/.bin/lv_font_conv" \
    --font "${font_path}" \
    --size 16 --bpp 4 --format lvgl --lv-include lvgl.h \
    --range 0x20-0x7E --symbols "${chars}" \
    --autohint-off \
    -o "${out_file}"
echo "Wrote ${out_file}"
