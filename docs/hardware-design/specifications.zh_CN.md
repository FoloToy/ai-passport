<p align="right">
  <strong>简体中文</strong> · <a href="specifications.md">English</a>
</p>

# 产品规格（Specifications）

本文定义面向用户的产品规格。固件引脚、总线和运行约束见[硬件开发指南](AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md)和 `components/bsp/include/bsp_pins.h`。

| 项目 | 规格 |
| --- | --- |
| 硬件基线 | 标准量产版本；所有出货量产设备均使用本仓库定义的引脚与器件映射 |
| 形态 | 可穿戴、透明外壳 |
| 尺寸 | 60 × 95 × 8.5 mm |
| 重量 | 50 g |
| MCU | ESP32-C3，8 MB Flash，无 PSRAM |
| 显示 | ST7789P3，240 × 320 彩色 TFT，RGB565 |
| 无线 | 2.4 GHz Wi-Fi 802.11 b/g/n；Bluetooth 5 LE |
| NFC | 被动 NTAG213，支持普通 NDEF 读写 |
| 功能输入 | 上、下、确定三枚按键，通过 GPIO0 ADC 电阻梯接入 |
| 电源输入 | 独立硬件电源键 |
| 休眠唤醒 | 任意功能键将 GPIO0 分压节点拉低，可从 light sleep 或 deep sleep 唤醒 ESP32-C3 |
| 电源行为 | 外部电源电路在持续按住 0.5 秒后开机、持续按住 2 秒后关机；固件无法读取电源键 |
| 音频 | 内置麦克风和扬声器，使用 ES8311 codec；功放无 MCU 使能脚，直接由电池供电 |
| 充电 | USB Type-C 2.0，5 V 输入 |
| 电池 | 内置 520 mAh 可充电锂电池；所有量产设备均安装 CW2017 电量计 |
