<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Fonts

Store reusable font files and generated font sources here.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

## Registered fonts

### font_sm_cjk_16 (SparkMinds Passport Chinese UI font)

- **Source**: Noto Sans CJK SC Regular, from
  `https://github.com/notofonts/noto-cjk` (`Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf`).
- **License**: SIL Open Font License 1.1 (embedding and subsetting permitted).
- **Integration**: subset and converted to an LVGL C-array font by
  `tools/gen_font.sh`; the generated `main/font_sm_cjk_16.c` is committed,
  the 16 MB source OTF is not (re-download per the script's instructions).
- **Character set**: ASCII printable range plus the CJK characters used by the
  SparkMinds Passport UI string literals (auto-extracted by the script).
- **Parameters**: 16 px, 4 bpp, no autohint; roughly 100 KB of Flash.
  Regenerate after changing any UI string.
