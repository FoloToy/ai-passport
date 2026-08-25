<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 硬件设计（Hardware Design）

本目录维护产品规格、固件可见的板级事实、资源约束、验收矩阵与排障知识。

## 目录约定

- 固件引脚和板级常量以 `components/bsp/include/bsp_pins.h` 为准。
- 产品规格维护在 `specifications.zh_CN.md`；固件行为与约束维护在硬件指南中。
- 不得根据 ESP32-C3 通用能力或其他开发板推断本板接口。
- 修改引脚、I2C、ADC、屏参或音频时钟等映射时，必须同步文档并记录实机结果。

## 文档地图

| 文档 | 读者与权威范围 |
| --- | --- |
| [AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md](AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md) | BSP 行为、资源所有权、约束、验收与排障参考。 |
| [specifications.zh_CN.md](specifications.zh_CN.md) | 对外产品规格。 |
| `components/bsp/include/bsp_pins.h` | 固件引脚、总线实例、地址、屏参和按键窗口的单一事实来源。 |

新文档需写明适用范围，通过链接引用软件接口以避免复制常量，并将构建结果与实机结果分开记录。
