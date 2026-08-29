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

> 当前为空骨架，用于存放后续加入的字库资源。加入资源时请同步更新本 `README.md` 的索引。

## 已登记字库

### font_sm_cjk_16（创智 Passport 中文 UI 字库）

- **来源**：Noto Sans CJK SC Regular，取自
  `https://github.com/notofonts/noto-cjk`（`Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf`）。
- **授权**：SIL Open Font License 1.1（允许嵌入与子集化）。
- **集成方式**：由 `tools/gen_font.sh` 子集化并转换为 LVGL C 数组字库；
  生成物 `main/font_sm_cjk_16.c` 入库，16 MB 源 OTF 不入库（按脚本提示重新下载）。
- **字符集**：ASCII 可打印区 + 创智 Passport UI 字符串字面量用到的 CJK 字符
  （脚本自动提取）。
- **参数**：16 px、4 bpp、关闭 autohint；约占 100 KB Flash。
  修改任何 UI 文案后需重新生成。
