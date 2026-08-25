<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 硬件设计（Hardware Design）

本目录用于存放本仓库的硬件设计文档，包括板级事实、引脚映射、硬件约束、验收矩阵与排障知识。

## 目录约定

- 硬件事实以 `components/bsp/include/bsp_pins.h` 为准，属于该文件的范围在此处只作引用，不重复。
- 硬件文档应区分「已确认事实」与「未知项」；未知项需明确报告并请求证据，不要用其它型号板子的参数补位。
- 每次修改硬件映射（引脚、I2C、ADC、屏参、音频时钟等）需同步更新本文档并记录实机结果。

## 如何添加一篇硬件设计文档

1. 在本目录下新建一个描述性文件（如 `xxx-hw.md`）或子目录。
2. 文档顶部写明适用板卡/版本与日期。
3. 与软件接口相关的结论引用 `docs/software-design/` 与仓库根 `AGENTS.md`。

## 现有文档索引

- [AI_HARDWARE_DEVELOPMENT_GUIDE.md](AI_HARDWARE_DEVELOPMENT_GUIDE.md)：完整的硬件开发指南与排障参考（上游已有，已归位到本目录）。
- [specifications.md](specifications.md)：产品规格（面向用户与产品的设备规格：尺寸、重量、电池、充电、NFC、按键等）。

> 注：`docs/hardware-design` 为本次仓库规范化目录，用于容纳硬件设计文档。上游既有硬件指南 `AI_HARDWARE_DEVELOPMENT_GUIDE.md` 已归入本目录。
