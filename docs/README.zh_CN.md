# FoloToy AI Passport

[English](README.md) | 简体中文

FoloToy AI Passport 是一个开放式可穿戴 AI 硬件，本仓库是这款 AI 硬件的开发基线。它不只展示"板子能运行什么"，还把开发应用所需的**硬件事实、稳定接口、资源边界、参考实现和验收方法**放在同一仓库中。

这个仓库的组织方式是：

- `main` 是最小但完整的可运行基线，也是当前硬件能力的可执行说明；
- `components/bsp` 隔离板级差异，为应用提供稳定 API；
- `demo/*` 分支展示从需求到成品的不同实现路径；
- AI 开发约定见 [`AGENTS.zh_CN.md`](../AGENTS.zh_CN.md) 与 [`docs/development/agent-guide.zh_CN.md`](development/agent-guide.zh_CN.md)；完整硬件上下文和故障知识见 [`docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md`](hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md)；
- 构建结果与真机结果分开记录，禁止把"编译通过"描述成"硬件验证通过"。

## 硬件能力契约

下表描述的是当前 `main` 已提供的应用能力，而不是芯片数据手册中所有可能的能力。

| 能力 | 已确认实现 | 应用接口 | 必须遵守的边界 |
| --- | --- | --- | --- |
| 显示 | ST7789P3，240 × 320，竖屏 RGB565，SPI2 40 MHz；LEDC 背光 | `bsp_display_*`、`bsp_lvgl_*` | ESP32-C3 无 PSRAM；当前为小型单 DMA 缓冲；BSP 未暴露 LCD MISO、触摸或 TE 接口 |
| 输入 | `UP` / `DOWN` / `OK` 三键，共用 GPIO0 的 ADC 电阻分压 | `bsp_button_init()`、`bsp_button_read_mv()` | 回调运行在 button 组件任务中，不能阻塞；不能再创建第二个 ADC1 unit |
| 音频 | ES8311，I2S0 全双工 PCM，可播放和麦克风录音 | `bsp_audio_*` | PCM 读写为阻塞调用，应放工作任务；格式切换必须保留 BSP 内的 close/open 流程 |
| 电池 | CW2017 的 SOC 与电压读取 | `bsp_battery_*` | 是可缺省能力；读数精度取决于电芯与 profile，不能等同于已标定结果 |
| Wi-Fi | 按需 2.4 GHz STA 扫描 demo | `main/demo_wifi.c` | 仅扫描；不连接、不存凭证、不验证天线/射频表现 |
| Bluetooth LE | 按需以 `FoloPassport` 名义做不可连接的 NimBLE 广播 | `main/demo_ble.c` | ESP32-C3 不支持蓝牙经典；射频范围、共存与功耗需实测 |
| 低功耗 | 两秒浅睡眠与五秒深睡眠，均以 RTC 定时器唤醒 | `main/demo_low_power.c` | 深睡眠会重启应用；当前 demo 只提供 RTC 定时器唤醒 |
| 共享总线 | ES8311 与 CW2017 共用 I2C0 | `bsp_i2c_*` | 所有设备复用 BSP 持有的总线；不能为扫描或新设备再创建同端口总线 |
| 日志与烧录 | ESP32-C3 原生 USB Serial/JTAG | ESP-IDF console | GPIO18/19 保留给 USB；UART0 默认 TX GPIO21 与背光冲突 |

所有引脚、地址、面板参数和按键电压窗口只在 [`components/bsp/include/bsp_pins.h`](../components/bsp/include/bsp_pins.h) 定义。应用代码不得复制这些常量。完整引脚表、面板初始化、ADC 阈值、I2C 地址规则、音频时钟和内存说明见 [AI 硬件开发指南](hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md)。

应用也可以使用 ESP-IDF 提供的定时器、FreeRTOS 任务和内部 Flash/NVS；番茄钟分支提供了 NVS 示例。Wi-Fi 和 Bluetooth LE 仍是 ESP-IDF 应用服务而非 BSP API：其菜单页面仅在打开时初始化对应协议栈、退出时释放。`demo/claude-buddy-port` 仍是更完整的 BLE 应用架构参考，不能替代对当前板卡天线、射频表现、功耗和共存行为的实测。当前产品与固件基线使用 8 MB Flash，包含 3 MB factory-app 分区，并固定保留设备身份与永久 Recovery 区域，使二创固件仍可通过小程序安装。

### 不属于当前能力契约的事项

公开固件能力以表中接口为限，不能仅凭 ESP32-C3 芯片能力推断其他板级接口。新增硬件接口必须提供明确的 BSP 定义和实机验收标准。

## 用一句需求开始开发

简单需求可以直接交给 AI 助手：

```text
请为 FoloToy AI Passport 开发一个离线习惯打卡应用。
使用三个实体按键和 240×320 屏幕，记录保存在掉电不丢失的存储中。
从 `main` 开始，创建 `feature/*` 分支并在该分支上开发。
遵守 AGENTS.md 和 docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md；先查找相关 demo 分支与 plays/ 应用，
保持硬件逻辑在 components/bsp、应用逻辑在 main，完成可运行实现与测试，
最后分别报告构建结果、未执行的真机项目和逐项验收方法。
```

开始前先看 [`plays/`](../plays/README.md) 有没有已存在或可参考的应用，以及相关 demo 分支，
再看 [`docs/development/experience-notes.md`](development/experience-notes.md) 里已沉淀、可复用的经验。
这些列出了已经构建好、可复用的东西。

需求越具体，AI 助手越容易一次实现正确。建议说明：

- 用户流程：每个页面显示什么，三个按键的短按、双击、长按分别做什么；
- 状态与数据：是否计时、断电保存、联网、录音或与电脑通信；
- 体验目标：字体、颜色、动画、声音、响应时间和异常状态；
- 限制条件：是否允许替换主菜单、增加依赖、使用 Flash 或改变默认交互；
- 验收标准：哪些行为必须自动测试，哪些必须在真实硬件观察。

若需求没有给出所有细节，AI 助手可以在不改变产品方向的范围内采用保守默认值，但应在交付中列出这些假设。涉及新接线、电源安全、硬件版本或不可恢复数据格式的决定必须先确认。

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

开始新应用。本仓库在同一个基线上承载多个独立项目：从 `main` 开始后，应创建 `feature/*` 分支并在该分支上开发，**不要**直接在 `main` 上开发。每个项目的最终分支都是 `feature/*`（如 `feature/my-passport-app`），让 `main` 保持干净的上游基线，各项目互不纠缠。

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
tools/                   本地与 CI 共用的验证及固件校验脚本
docs/                    项目说明、变更记录、工程/协作规范与设计参考
.github/                 GitHub 社区文档、PR 模板、Issue Form 与 CI 工作流
sdkconfig.defaults       ESP32-C3、USB console、Flash、LVGL 默认配置
partitions.csv           应用与设备身份/Recovery 保护分区布局
dependencies.lock        可复现的 ESP-IDF Managed Component 解析结果
AGENTS.md                AI agent 必读入口（与 AGENTS.zh_CN.md 配对）
CLAUDE.md                Claude Code 指向 AGENTS.md 的入口（含中文配对）
LICENSE                  仓库许可证
```

## 文档索引

- [`docs/INDEX.zh_CN.md`](INDEX.zh_CN.md) — 全部文档索引（协作规范、工程规范、fork 工作流、软硬件设计）。
- [`docs/development/agent-guide.zh_CN.md`](development/agent-guide.zh_CN.md) — AI 开发工作流（上下文建立、事实来源优先级、BSP 边界、运行时规则、交付格式）。
- [`docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md`](hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md) — 硬件开发指南（引脚表、验收矩阵、故障速查）。
- [`AGENTS.zh_CN.md`](../AGENTS.zh_CN.md) — AI 协作规范入口。
- [`docs/fork-guide.zh_CN.md`](fork-guide.zh_CN.md) — fork 工作流。

> 注：本 README 只描述产品与仓库，不含给 AI 的执行说明；AI 开始开发前请先读根目录 `AGENTS.zh_CN.md`，再按任务路由读取相关文档。
