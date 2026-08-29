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

`main/lv_font_cn_16.c` is generated from Source Han Sans SC Regular at 16 px,
2 bpp. It contains ASCII plus the GB2312 level-one character set. Source Han
Sans is distributed under the SIL Open Font License 1.1.

Regenerate it with Pillow and a local copy of the OFL font:

```bash
python3 tools/gen_safety_font.py \
  --font /path/to/SourceHanSansSC-Regular.otf --size 16
```

The generated C source is compiled from `main/` and stored in Flash as read-only
glyph data. Unsupported characters are rendered as `?` rather than an empty
glyph box.
