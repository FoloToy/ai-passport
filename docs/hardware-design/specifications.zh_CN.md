<p align="right">
  <strong>简体中文</strong> · <a href="specifications.md">English</a>
</p>

# 产品规格（Specifications）

> **定位**：面向产品与用户的设备规格（对外口径）。
> **读取时机**：需要引用设备尺寸、重量、电池、充电、NFC、按键等用户可见规格时。
> **与工程文档的关系**：芯片、引脚、总线、接口等工程事实以 `docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md` 与 `components/bsp/include/bsp_pins.h` 为准；本文档承载产品规格口径。

以下为设备产品规格（源自官方产品页）：

| 项目 | 规格 |
| --- | --- |
| 形态 | 可穿戴，透明外壳（屏幕 / 主板 / NFC / 麦克风 / 扬声器 / 电池均为产品语言一部分） |
| 尺寸 | 60 × 95 × 8.5 mm |
| 重量 | 50 g |
| MCU | ESP32-C3（8MB Flash） |
| 显示 | 240 × 320 彩色 TFT |
| 无线 | 2.4 GHz Wi-Fi（802.11 b/g/n）；Bluetooth® 5 LE（可同步头像 / 昵称 / Token / 个性化内容） |
| NFC | 被动 NFC 标签（NTAG213，支持读卡器 / 手机读写普通 NDEF 数据） |
| 输入 | 上 / 下 / 确定三枚功能键 + 独立电源键（硬件实现，不可改功能） |
| 电源 | 按住电源键 0.5s 开机；长按约 2s 关机；自动息屏后按任意功能键唤醒 |
| 音频 | 内置麦克风 + 内置扬声器 |
| 充电 | USB Type-C 2.0 5V |
| 电池 | 内置 520 mAh 可充电锂电池 |
| 其他 | 专属二维码（QR fallback，含恢复固件入口） |
