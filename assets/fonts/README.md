<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Fonts

Store reusable font files and generated font sources here.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

## Senior Safety Card embedded font

`main/lv_font_cn_16.c` and `main/lv_font_cn_22.c` are 2 bpp fonts generated
from Source Han Sans SC Regular and Bold. Both contain ASCII, the GB2312
level-one character set, and common Chinese punctuation. The 22 px bold font is
used for the two primary emergency pages. Source Han Sans is distributed under
the SIL Open Font License 1.1.

Regenerate it with Pillow and a local copy of the OFL font:

```bash
python3 tools/gen_safety_font.py \
  --font /path/to/SourceHanSansSC-Regular.otf --size 16
python3 tools/gen_safety_font.py \
  --font /path/to/SourceHanSansSC-Bold.otf --size 22
```

The generated C source is compiled from `main/` and stored in Flash as read-only
glyph data. Unsupported characters are rendered as `?` rather than an empty
glyph box.
