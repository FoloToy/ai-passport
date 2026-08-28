<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 硬件设计

本目录包含适用于每台标准量产 AI Passport 的权威硬件文档。

## 文档

- [specifications.zh_CN.md](specifications.zh_CN.md)：面向用户的尺寸、器件、电源、输入和电池规格。
- [AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md](AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md)：固件可见引脚、所有权、电气事实、约束、排障和实机验收。
- [`bsp_pins.h`](../../components/bsp/include/bsp_pins.h)：BSP 代码使用的固件常量。

不得只根据 ESP32-C3 数据手册推断板级接口。硬件修改必须在同一个 PR 中更新 `bsp_pins.h`、硬件指南和对应实机验收结果。
