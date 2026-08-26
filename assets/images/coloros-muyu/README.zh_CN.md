<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# ColorOS 木鱼图片资源

- `reference-hit-240x320.png`：鼓槌处于敲击状态的已确认界面参考图。
- `background-240x320.png`：固件使用的无文字、无鼓槌背景源图。
- `main/assets/coloros_muyu_bg.rgb565`：生成的 240 × 320 RGB565LE 固件资源。
- `main/font_muyu_22.c`：22 px、4 bpp 的思源黑体精简字体，仅包含界面所需字符。

从 PNG 重新生成固件背景：

```bash
ffmpeg -i assets/images/coloros-muyu/background-240x320.png \
  -pix_fmt rgb565le -f rawvideo main/assets/coloros_muyu_bg.rgb565
```

纯背景以已确认参考图为编辑目标，通过内置 ImageGen 生成。编辑时移除了
全部文字、鼓槌和敲击线，同时保留木鱼、橙色菱形纹理以及红金波浪。
