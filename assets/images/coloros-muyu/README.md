<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# ColorOS Wooden Fish assets

- `reference-hit-240x320.png`: approved layout reference with the striker at impact.
- `background-240x320.png`: text-free and striker-free source used by the firmware.
- `main/assets/coloros_muyu_bg.rgb565`: generated 240 x 320 RGB565LE firmware asset.
- `main/font_muyu_22.c`: 22 px, 4 bpp Source Han Sans SC subset containing only the UI glyphs.

Regenerate the firmware background from the PNG:

```bash
ffmpeg -i assets/images/coloros-muyu/background-240x320.png \
  -pix_fmt rgb565le -f rawvideo main/assets/coloros_muyu_bg.rgb565
```

The clean background was edited with built-in ImageGen from the approved reference.
It removes all text, the striker, and impact marks while preserving the wooden fish,
orange diamond texture, and red/gold waves.
