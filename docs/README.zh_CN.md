# FoloToy AI Passport

[English](README.md) | 简体中文

FoloToy AI Passport 是一个开放式可穿戴 AI 硬件，本仓库是这款 AI 硬件的开发基线。它不只展示"板子能运行什么"，还把开发应用所需的**硬件事实、稳定接口、资源边界、参考实现和验收方法**放在同一仓库中。

这个仓库的组织方式是：

- `main` 是最小但完整的可运行基线，也是当前硬件能力的可执行说明；
- `components/bsp` 隔离板级差异，为应用提供稳定 API；
- `demo/*` 分支展示从需求到成品的不同实现路径；
- AI 开发约定见 [`AGENTS.md`](../AGENTS.md) 与 [`docs/development/agent-guide.md`](development/agent-guide.md)；完整硬件上下文和故障知识见 [`docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md`](hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md)；
- 构建结果与真机结果分开记录，禁止把"编译通过"描述成"硬件验证通过"。

## 硬件能力契约

下表描述的是当前 `main` 已提供的应用能力，而不是芯片数据手册中所有可能的能力。

| 能力 | 已确认实现 | 应用接口 | 必须遵守的边界 |
| --- | --- | --- | --- |
| 显示 | ST7789P3，240 × 320，竖屏 RGB565，SPI2 40 MHz；LEDC 背光 | `bsp_display_*`、`bsp_lvgl_*` | ESP32-C3 无 PSRAM；当前为小型单 DMA 缓冲；没有 LCD MISO、触摸或已知 TE 接口 |
| 输入 | `UP` / `DOWN` / `OK` 三键，共用 GPIO0 的 ADC 电阻分压 | `bsp_button_init()`、`bsp_button_read_mv()` | 回调运行在 button 组件任务中，不能阻塞；不能再创建第二个 ADC1 unit |
| 音频 | ES8311，I2S0 全双工 PCM，可播放和麦克风录音 | `bsp_audio_*` | PCM 读写为阻塞调用，应放工作任务；格式切换必须保留 BSP 内的 close/open 流程 |
| 电池 | CW2017 的 SOC 与电压读取 | `bsp_battery_*` | 是可缺省能力；读数精度取决于电芯与 profile，不能等同于已标定结果 |
| 共享总线 | ES8311 与 CW2017 共用 I2C0 | `bsp_i2c_*` | 所有设备复用 BSP 持有的总线；不能为扫描或新设备再创建同端口总线 |
| 日志与烧录 | ESP32-C3 原生 USB Serial/JTAG | ESP-IDF console | GPIO18/19 保留给 USB；UART0 默认 TX GPIO21 与背光冲突 |

所有引脚、地址、面板参数和按键电压窗口只在 [`components/bsp/include/bsp_pins.h`](../components/bsp/include/bsp_pins.h) 定义。应用代码不得复制这些常量。完整引脚表、面板初始化、ADC 阈值、I2C 地址规则、音频时钟和内存说明见 [AI 硬件开发指南](hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md)。

应用也可以使用 ESP-IDF 提供的定时器、FreeRTOS 任务和内部 Flash/NVS；番茄钟分支提供了 NVS 示例。ESP32-C3 芯片支持 2.4 GHz Wi-Fi 和 Bluetooth LE，但当前 BSP 没有为无线能力提供封装，`main` 也不初始化无线栈；`demo/claude-buddy-port` 只能作为 BLE 应用架构参考，不能替代对当前板卡天线、射频表现、功耗和共存行为的实测。所有 FoloToy AI Passport 均配备 8 MB Flash，默认固件配置也以 8 MB 为准。

### 不属于当前能力契约的事项

仓库目前没有足够证据保证以下能力：触摸、屏幕读回、IMU、外部存储、充电控制、USB 插拔检测、可控功放使能、深度睡眠唤醒、任意"空闲 GPIO"、电池精确容量或量产级电源指标。ESP32-C3 芯片具备某项功能，不代表这块板已经接出、供电正确或经过验证。

需要这些能力时，先补充原理图、板卡修订号、器件资料或实测结果，再扩展 BSP 和验收项。

## 示例分支是设计案例，不是功能堆叠

每个 `demo/*` 分支都从基线演化出一个独立应用。它们的价值是展示具体问题的实现方式；新应用通常应从 `main` 建分支，按需参考，而不是把多个 demo 整体合并。

| 分支 | 展示的应用 | 值得复用的模式 |
| --- | --- | --- |
| `demo/stopwatch` | 秒表 | 最小计时应用、纯逻辑与 LVGL 分离、主机逻辑测试 |
| `demo/cat-themed-pomodoro-timer` | 猫咪养成番茄钟 | 单调时钟、暂停/恢复、NVS 持久化、较完整的 PRD 与状态模型 |
| `demo/rock-paper-scissors` | 石头剪刀布 | RGB565 图片资产、素材生成脚本、Flash 资源权衡 |
| `demo/tetris-game` | 三键俄罗斯方块 | 实时游戏循环、低延迟 `PRESS` 输入、局部刷新、纯游戏模型、音效与麦克风交互 |
| `demo/claude-buddy-port` | 桌面 AI 硬件伴侣 | 用完整应用替换 demo 菜单、加密 BLE、协议解析、状态归约、任务通信和较完整的主机测试 |

查看示例而不切换当前工作区：

```bash
git branch -r --list 'origin/demo/*'
git diff main...origin/demo/tetris-game -- main components tests
git show origin/demo/tetris-game:main/demo_tetris.c
```

开始新应用：

```bash
git switch main
git switch -c feature/my-passport-app
```

示例分支之间可能改变了同一菜单、配置或驱动。应先理解差异，再提取状态模型、资源流水线或并发模式；不能因为代码曾出现在示例分支，就把它当成当前 `main` 的 BSP 保证。

## 项目结构

```text
components/bsp/include/  BSP 公开 API 与 bsp_pins.h 硬件事实
components/bsp/src/      显示、按键、音频、电池、共享 I2C 实现
main/                    最小菜单、LVGL UI 与独立硬件演示页
tests/                   可脱离硬件运行的轻量逻辑测试源
docs/                    工程/协作规范、设计文档、CI 文档与 README（README.md / README.zh_CN.md / INDEX.md）
.github/workflows/       CI 工作流（build-firmware.yml、sync-main.yml）
sdkconfig.defaults       ESP32-C3、USB console、Flash、LVGL 默认配置
AGENTS.md                agent 在本仓库的编码、验证和提交规则
CONTRIBUTING.md  贡献指南
CODE_OF_CONDUCT.md  贡献者公约行为准则
SECURITY.md      安全漏洞报告
SUPPORT.md       使用支持渠道
```

## 文档索引

- [`docs/INDEX.md`](INDEX.md) — 全部文档索引（协作规范、工程规范、fork 工作流、软硬件设计）。
- [`docs/development/agent-guide.md`](development/agent-guide.md) — AI 开发工作流（面向 AI 编程助手：上下文建立、需求拆解、BSP 边界、验收交付格式）。
- [`docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md`](hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md) — 硬件开发指南（引脚表、验收矩阵、故障速查）。
- [`AGENTS.md`](../AGENTS.md) — AI 协作规范入口。
- [`docs/fork-guide.md`](fork-guide.md) — fork 工作流。

> 注：本 README 只描述产品与仓库，不含给 AI 的执行说明；AI 开始开发前请先读 `docs/development/agent-guide.md`。
