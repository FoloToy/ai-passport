<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# FoloToy AI Passport

本仓库是所有标准量产版 FoloToy AI Passport 的自包含固件开发基线。AI 编程 Agent 必须只使用当前 checkout 完成 ESP-IDF 5.5.3 环境搭建、应用实现、主机检查、ESP32-C3 固件编译和合并镜像输出，不得依赖远程 demo 分支。

## 硬件能力契约

`main` 为每项基线能力提供一个可运行页面。`components/bsp` 管理可复用板级行为和全部固件可见硬件常量。

| 能力 | 基线实现 | 必须遵守的边界 |
| --- | --- | --- |
| 显示 | ST7789P3，240 × 320 RGB565，SPI2 40 MHz，LEDC 背光 | 使用 `bsp_display_*` 和 `bsp_lvgl_*`；没有 PSRAM、LCD MISO、触摸或 TE 接口 |
| 按键 | 量产实测 GPIO0 ADC 电阻梯上的 UP/DOWN/OK | 使用 `bsp_button_*`；回调不得阻塞；ADC1 只能有一个所有者 |
| 音频 | ES8311 通过 I2S0 全双工播放与麦克风录音 | 阻塞式 PCM I/O 放入工作任务；保留 BSP 的格式 close/open 流程 |
| 电池 | 必装 CW2017 的 SOC 与电压读取 | `ESP_ERR_NOT_FOUND` 表示硬件故障；读数不代表已标定电芯容量 |
| Wi-Fi | 2.4 GHz station 扫描 | demo 不连接网络、不保存凭证 |
| Bluetooth LE | 以 `FoloPassport` 进行不可连接 NimBLE 广播 | 不支持 Bluetooth Classic；范围与功耗由实机测试确定 |
| 低功耗 | GPIO0 按键或 RTC timer 唤醒 light/deep sleep | 任意功能键可唤醒；light sleep 的 timer 为 2 秒，deep sleep 为 5 秒 |
| 共享 I2C | ES8311 `0x18` 与 CW2017 `0x63` 共用 I2C0 | 复用 `bsp_i2c`；禁止在 I2C0 创建第二条总线 |
| 烧录 | 原生 USB Serial/JTAG 与从 0x0 烧录的合并镜像 | GPIO18/19 保留给 USB；GPIO21 保留给背光 |

完整引脚表、电气事实、资源所有权与实机验收矩阵见[硬件指南](hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md)。应用代码不得复制 [`bsp_pins.h`](../components/bsp/include/bsp_pins.h) 中的常量。

## 用一个需求交给 AI Agent

需求必须包含可观察行为与验收标准：

```text
为 FoloToy AI Passport 开发一个离线习惯打卡应用。
使用三个按键和 240×320 屏幕，记录保存在 NVS。
UP/DOWN 选择日期；OK 切换完成状态；OK 长按返回菜单。
遵守 AGENTS.md。板级逻辑放 components/bsp，应用逻辑放 main。
运行完整验证门禁并输出 build/FoloToy-AI-Passport-full.bin。
分别报告 Build、Host tests、Device tests 和 Unverified。
```

需求必须定义用户流程、按键行为、持久化数据、联网/音频用途、资源限制和实机验收。缺少任何会改变可观察行为的决定时，Agent 必须在实现前询问用户。Agent 不得编造接线、引脚、电气限制、板卡版本、凭证或不可恢复的存储格式。

## 项目结构

```text
components/bsp/include/  BSP 公开 API 与 bsp_pins.h 硬件事实
components/bsp/src/      显示、按键、音频、电池和共享 I2C 实现
main/                    可运行菜单、LVGL UI 与七个基线 demo 页面
tests/                   脱离硬件运行的主机测试
tools/                   环境无关检查与固件验证脚本
docs/                    产品、工程、贡献与硬件文档
.github/                 社区规范、模板与三个 CI workflow
sdkconfig.defaults       ESP32-C3、8 MB Flash、USB、休眠和 LVGL 默认配置
partitions.csv           NVS、PHY data 与一个 3 MB factory application
dependencies.lock        锁定的 ESP-IDF Managed Component 解析结果
AGENTS.md                AI Agent 唯一必读入口
CLAUDE.md                Claude Code 指向 AGENTS.md 的入口
```

## 权威入口

- [`AGENTS.zh_CN.md`](../AGENTS.zh_CN.md)：强制任务路由、安全规则与交付字段。
- [`development/agent-guide.zh_CN.md`](development/agent-guide.zh_CN.md)：代码修改流程与运行时不变量。
- [`development/environment-setup.zh_CN.md`](development/environment-setup.zh_CN.md)：全新机器 ESP-IDF 5.5.3 环境搭建。
- [`development/build-and-test.zh_CN.md`](development/build-and-test.zh_CN.md)：准确检查、固件输出、CI 与实机测试边界。
- [`hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md`](hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md)：已确认板级事实与实机验收。

仅在发现其它维护中文档时使用 [`INDEX.zh_CN.md`](INDEX.zh_CN.md)。
