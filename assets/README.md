<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, sound effects, and battery profiles, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

Store reusable font files and generated font sources in `fonts/`.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

### Lucky Draw app font subsets (2026-11-24)

| File | Size | Character range | Generated size |
| --- | --- | --- | --- |
| [`fonts/lucky_font_16.c`](fonts/lucky_font_16.c) | 16 px | ASCII 0x20-0x7E + the Chinese glyphs used by the app UI (see the Chinese entry for the full list) + U+00B7 middle dot + U+25B2/U+25BC triangles | ~68 KB |
| [`fonts/lucky_font_24.c`](fonts/lucky_font_24.c) | 24 px | Same character set (page title, ball digits) | ~119 KB |
| [`fonts/lucky_font_40.c`](fonts/lucky_font_40.c) | 40 px | ASCII 0x20-0x3F (setup-page large counter) | ~47 KB |

- Source font: [`fonts/NotoSansCJKsc-Regular.otf`](fonts/NotoSansCJKsc-Regular.otf) (Noto Sans CJK SC Regular, aka Source Han Sans SC).
- License: SIL Open Font License 1.1; redistribution permitted. Full license text lives in the upstream repository (https://github.com/notofonts/noto-cjk).
- Conversion tool: lv_font_conv 1.5.3 (pinned, invoked via npx).
- Conversion command (16 px shown; swap `--size`/`--lv-font-name` for the other sizes, and use `--range 0x20-0x3F` for the 40 px digit-only subset). The `--symbols` argument is written with bash ANSI-C Unicode escapes to keep this file free of CJK prose; the Chinese version lists the same glyphs literally:

  ```bash
  npx lv_font_conv@1.5.3 \
    --font assets/fonts/NotoSansCJKsc-Regular.otf \
    --size 16 --bpp 4 --format lvgl --no-compress \
    --lv-font-name lucky_font_16 --lv-include lvgl.h \
    --range 0x20-0x7E \
    --symbols $'\u5FEB\u4E50\u9009\u53F7\u9009\u62E9\u53F7\u7801\u4E2A\u6570\u4E0A\u4E0B\u8C03\u6574\u786E\u5B9A\u5F00\u59CB\u672C\u7EC4\u518D\u62BD\u957F\u6309\u8FD4\u56DE\u53CC\u8272\u7403\u7EA2\u84DD\u6362\u73A9\u6CD5\u5F00\u5956\u7ED3\u679C\u671F\u7FFB\u5237\u65B0\u672A\u914D\u7F6E\u7F51\u7EDC\u52A0\u8F7D\u4E2D\u62C9\u53D6\u5931\u8D25\u6682\u65E0\u6570\u636E\u8FDE\u63A5\u770B\u952E\u5207\u4EA4\u4FDD\u51FA\u52A8\u540E\u542F\u5B58\u5DF2\u5F0F\u5F85\u624B\u673A\u6A21\u70B9\u70ED\u7B49\u81EA\u9000\u91CD\u63D0\u25B2\u25BC\u00B7' \
    --output assets/fonts/lucky_font_16.c
  ```

- Integration: compiled via `target_sources` in `main/CMakeLists.txt`; widgets bind them through `LV_FONT_DECLARE` + `lv_obj_set_style_text_font`. When UI copy changes, regenerate every affected size and re-run `./tools/validate.sh --static` (`tools/check_glyphs.py` scans `main/` string literals against glyph coverage).

## Images

Store reusable source images and generated display assets in `images/`.

| File | Dimensions and format | Use and source |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160, JPEG | Product hero image embedded in both project README files to foreground AI Passport and its open, maker-oriented identity. |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724, PNG RGBA | Optional technical infographic retained as a reference asset; it is no longer used as the homepage hero. Generated for this repository with the built-in image generation tool on 2026-09-17; the six labels and values were checked against the documented hardware contract. |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336, PNG RGBA | Transparent black wordmark extracted from the repository's original `images/logo.png`; embedded in both project README files for light backgrounds. |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336, PNG RGBA | White version of the extracted wordmark, used by the README `<picture>` element when GitHub is in dark mode. |

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

## Music and sound effects

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.

## Battery profiles

[`battery/wanli-500mah-profile2-xil40x-20261020.txt`](battery/wanli-500mah-profile2-xil40x-20261020.txt)
contains the supplied CW2017 profile for the Wanli New Energy 500 mAh cell
(profile2, XIL40X, 2026-10-20). It is an unchanged copy of the project-supplied
file, retained at the maintainer's request for this firmware update. Its
original filename is recorded in the paired Chinese document. No separate
license was included in the supplied file.

The format is 80 lines of two hexadecimal digits, one byte per line in register
order. The BSP embeds those bytes in `components/bsp/src/bsp_battery.c`; the
host gate checks the source file and initializer for an exact match. Text-file
SHA-256: `7ea96eba4634f4495a37c25ffacd54c1deaa0693b162c892d671a2ba68460919`.
Decoded 80-byte SHA-256:
`fa7365671093ef449c5bdcfb4adb279e3d275f888109f50b6b2f21e9b96220d5`.
This is a fuel-gauge profile, not a charging configuration or a generic profile
for every 500 mAh cell; see the [hardware guide](../docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md#9-cw2017-fuel-gauge).
