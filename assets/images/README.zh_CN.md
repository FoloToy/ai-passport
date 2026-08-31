<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 图片资源（Images）

本目录存放项目可复用的图片资源，如 UI 图标、背景、RGB565 资源等。

## 如何使用

- 图片文件复制到本目录，并在本项目 `README.md` 记录分辨率、格式、用途与来源。
- 与固件集成时，参考 [`components/bsp/include/bsp_display.h`](../../components/bsp/include/bsp_display.h) 与相关示例分支的图片资源管线，转换为固件所需格式（如 RGB565 数组）。
- 图片资源占用 Flash 与内存，集成前请评估 ESP32-C3 无 PSRAM 的限制。

## 目录说明

## 老人安心牌封面

`senior-safety-card-cover.png` 是 240 × 320（3:4）的社区发布封面，由实体设备通过 USB
直接导出 LVGL framebuffer 得到。它是真机验证后的第一个资料页，不是相机照片或硬件效果图。
所选页面不包含电话号码、二维码内容、凭证或其他私人联系方式。该图片只用于发布，不会编译进固件。
