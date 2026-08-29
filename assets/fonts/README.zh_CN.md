<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 字库资源（Fonts）

本目录存放项目可复用的字库资源。每个字库子目录或单个字库文件，应附说明。

## 如何使用

- 字库文件（如 `.ttf`、`.otf`、LVGL 使用的 C 数组字库等）复制到本目录，并在本项目 `README.md` 记录字名、字号、支持字符集与版权信息。
- 若需集成到 ESP-IDF 固件，参考 [`components/bsp/include/bsp_display.h`](../../components/bsp/include/bsp_display.h) 与 LVGL 字体接口，将字库转换为对应格式并放入正确资源目录。
- 字库占用 Flash 与内存，需在集成前评估 ESP32-C3 无 PSRAM 的限制（详见 `docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md`）。

## 目录说明

## 老人安心牌嵌入字库

`main/lv_font_cn_16.c` 由 Source Han Sans SC Regular（思源黑体）生成，字号 16 px、
2 bpp，字符集为 ASCII 加 GB2312 一级汉字。思源黑体采用 SIL Open Font License 1.1。

安装 Pillow 并准备一份本地 OFL 字体后，可重新生成：

```bash
python3 tools/gen_safety_font.py \
  --font /path/to/SourceHanSansSC-Regular.otf --size 16
```

生成的 C 源码从 `main/` 编译，字形数据以只读形式存入 Flash。字库不支持的字符会显示为 `?`，
不会显示成空白方框。
