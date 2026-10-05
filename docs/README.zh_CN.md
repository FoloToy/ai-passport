[English](/docs/README.md) · **简体中文**

<h1 align="center">FoloToy AI PASSPORT</h1>

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="../assets/images/logo-wordmark-dark.png">
    <img src="../assets/images/logo-wordmark.png" alt="FoloToy 字标" width="128">
  </picture>
</p>

<p align="center">
  <strong>戴上它，刷入固件，把它变成任何你想要的东西。</strong><br>
  简单开放，人人可造。
</p>

<p align="center">
  <a href="/docs/README.zh_CN.md"><img src="https://img.shields.io/badge/Open-firmware-14b8a6?style=flat-square" alt="开放固件"></a>
  <a href="/docs/README.zh_CN.md"><img src="https://img.shields.io/badge/Wearable-AI-2563eb?style=flat-square" alt="可穿戴 AI"></a>
  <a href="/docs/development/ai-guide.zh_CN.md"><img src="https://img.shields.io/badge/Built-for_makers-f97316?style=flat-square" alt="为创作者而造"></a>
  <a href="/LICENSE"><img src="https://img.shields.io/badge/License-MIT-64748b?style=flat-square" alt="MIT 许可证"></a>
</p>

<p align="center">
  <a href="https://ai-passport.folotoy.cn/">产品官网</a> ·
  <a href="#用一句需求开始开发">开始开发</a> ·
  <a href="/docs/reference/README.zh_CN.md">社区作品</a> ·
  <a href="#文档索引">开发文档</a>
</p>

---

**FoloToy AI Passport** 是开放的可穿戴 AI 平台，人人都可以动手改造、自由创作。
从一个简单想法开始，打造专属体验——无论是随身伙伴、小工具、游戏，还是任何新点子。

<p align="center">
  <img src="../assets/images/home.jpg" alt="FoloToy AI Passport 可穿戴设备的正面、侧面和背面展示。" width="100%">
</p>

| 开放自由，随心改造 | 人人都能开始 | 创造专属玩法 |
| --- | --- | --- |
| 开放固件与可复用示例，为你的创意留出发挥空间。 | 从一个简单想法出发，跟随清晰指南把它变成现实。 | 打造随身伙伴、小工具、游戏，或任何你能想到的东西。 |

## 找到你的起点

| 我想要…… | 从这里开始 |
| --- | --- |
| 使用设备、体验官方玩法 | [快速上手](https://ai-passport.folotoy.cn/guides/getting-started/) · [官方玩法](https://ai-passport.folotoy.cn/plays/) |
| 让 AI 开发自定义应用 | [Agent 规范](../AGENTS.zh_CN.md) · [AI 开发指南](development/ai-guide.zh_CN.md) · [必需技能](../skills/README.zh_CN.md) |
| 准备环境、编译固件 | [环境准备](/docs/development/engineering/environment-setup.zh_CN.md) · [构建与测试](development/engineering/build-and-test.zh_CN.md) |
| 了解硬件、参与贡献 | [硬件指南](hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md) · [贡献指南](../.github/CONTRIBUTING.zh_CN.md) |

> [!IMPORTANT]
> `main` 是最小可运行的**硬件测试基线**，不是成品应用。
> 二次开发必须重新设计 UI，禁止沿用当前 demo 测试菜单和页面；
> BSP API 与非 UI 逻辑仍可复用。

## 用一句需求开始开发

1. 在 AI 编程工具中打开仓库，让它先阅读 [`AGENTS.md`](../AGENTS.md)。
2. 由 AI 自行检查、安装[五个必需技能](../skills/README.zh_CN.md)，按当前环境要求取得必要权限。
3. 描述你想做的应用，从 `main` 创建新的 `feature/*` 分支开始开发。

复制这段提示词，替换成你的创意：

```text
请为 FoloToy AI Passport 开发一个离线习惯打卡应用。
使用三个实体按键和 240×320 屏幕，记录保存在掉电不丢失的存储中。
从 `main` 开始，创建 `feature/*` 分支并在该分支上开发。
遵守 AGENTS.md 和 docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md。
先查找相关 demo 分支与 docs/reference/ 应用档案。
硬件逻辑放在 components/bsp，应用逻辑放在 main。
完成可运行实现与测试，分别报告构建结果、未执行的真机项目和逐项验收方法。
必须重新设计应用 UI，禁止使用当前 demo 测试菜单和页面。
```

开始前先看 [`docs/reference/`](reference/README.zh_CN.md) 中已有的应用与开发经验，
再配合相关 demo 分支，确认哪些实现可以参考和复用。

<details>
<summary><strong>把需求说得更清楚</strong> — 页面、按键、数据与验收</summary>

需求越具体，AI 助手越容易一次实现正确。建议说明：

- 用户流程：每个页面显示什么，三个按键的短按、双击、长按分别做什么；
- 状态与数据：是否计时、断电保存、联网、录音或与电脑通信；
- 体验目标：字体、颜色、动画、声音、响应时间和异常状态；
- 限制条件：应用导航与按键设计、允许的依赖以及 Flash／数据使用范围；基线测试菜单不能作为应用 UI 选项；
- 验收标准：哪些行为必须自动测试，哪些必须在真实硬件观察。

若需求没有给出所有细节，AI 助手可以在不改变产品方向的范围内采用保守默认值，但应在交付中列出这些假设。涉及新接线、电源安全、硬件版本或不可恢复数据格式的决定必须先确认。

</details>

> [!NOTE]
> 编译通过不等于硬件验证通过。实现完成后应在真实设备上验收，刷写需要获得用户同意。
> 交付固件须为已校验、可从 `0x0` 烧录的合并 `full.bin`。
> 无需备份设备原有固件，但合并烧录可能重置已存数据，详见[烧录说明](development/engineering/firmware-layout.zh_CN.md#烧录与已存数据)。

## 示例分支是设计案例，不是功能堆叠

每个 `demo/*` 分支都从基线演化出一个独立应用。它们的价值是展示具体问题的实现方式；新应用通常应从 `main` 建分支，按需参考，而不是把多个 demo 整体合并。

`main` 上的菜单和 `demo_*.c` 页面只是硬件能力测试界面，不是应用 UI。所有二次开发应用都必须重新设计并实现页面与交互流程，禁止使用当前测试菜单、页面或界面外壳；改名、换颜色不算满足要求。BSP API、普通 LVGL 控件、生命周期模式和独立逻辑仍可复用。详见[强制 UI 重新设计规则](development/ai-guide.zh_CN.md#二次开发-ui-强制重新设计)；维护基线硬件测试 demo 本身属于另一类任务。

| 分支 | 展示的应用 | 值得复用的模式 |
| --- | --- | --- |
| `demo/stopwatch` | 秒表 | 最小计时应用、纯逻辑与 LVGL 分离、主机逻辑测试 |
| `demo/cat-themed-pomodoro-timer` | 猫咪养成番茄钟 | 单调时钟、暂停/恢复、NVS 持久化、较完整的 PRD 与状态模型 |
| `demo/rock-paper-scissors` | 石头剪刀布 | RGB565 图片资产、素材生成脚本、Flash 资源权衡 |
| `demo/tetris-game` | 三键俄罗斯方块 | 实时游戏循环、低延迟 `PRESS` 输入、局部刷新、纯游戏模型、音效与麦克风交互 |
| `demo/claude-buddy-port` | 桌面 AI 硬件伴侣 | 用完整应用替换 demo 菜单、加密 BLE、协议解析、状态归约、任务通信和较完整的主机测试 |

<details>
<summary><strong>查看示例并创建应用分支</strong></summary>

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

</details>

## 硬件能力契约

**ESP32-C3 · 8 MB Flash · 无 PSRAM · 240 × 320 屏幕 · 三个实体按键**

默认分区仅包含 **NVS、PHY data 和占用剩余 Flash 的单个 factory 应用**。
用户固件可按需调整为其他合法的 8 MB 布局，详见[固件布局](development/engineering/firmware-layout.zh_CN.md)。

<details>
<summary><strong>展开完整能力表</strong> — 接口、限制与实现细节</summary>

下表描述的是当前 `main` 已提供的应用能力，而不是芯片数据手册中所有可能的能力。

| 能力 | 已确认实现 | 应用接口 | 必须遵守的边界 |
| --- | --- | --- | --- |
| 显示 | ST7789P3，240 × 320，竖屏 RGB565，SPI2 40 MHz；LEDC 背光 | `bsp_display_*`、`bsp_lvgl_*` | ESP32-C3 无 PSRAM；当前为小型单 DMA 缓冲；BSP 未暴露 LCD MISO、触摸或 TE 接口 |
| 输入 | `UP` / `DOWN` / `OK` 三键，共用 GPIO0 的 ADC 电阻分压 | `bsp_button_init()`、`bsp_button_read_mv()` | 回调运行在 button 组件任务中，不能阻塞；不能再创建第二个 ADC1 unit |
| 音频 | ES8311，I2S0 全双工 PCM，支持播放、麦克风录音及软件休眠/恢复 | `bsp_audio_*` | PCM 读写为阻塞调用，应放工作任务；codec 休眠前必须停止 PCM I/O；格式切换必须保留 BSP 内的 close/open 流程 |
| 电池 | CW2017 的 SOC 与电压读取 | `bsp_battery_*` | 是可缺省能力；读数精度取决于电芯与 profile，不能等同于已标定结果 |
| Wi-Fi | 按需 2.4 GHz STA 扫描 demo | `main/demo_wifi.c` | 仅扫描；不连接、不存凭证、不验证天线/射频表现 |
| Bluetooth LE | 按需以 `FoloPassport` 名义做不可连接的 NimBLE 广播 | `main/demo_ble.c` | ESP32-C3 不支持蓝牙经典；射频范围、共存与功耗需实测 |
| 低功耗 | 两秒浅睡眠与五秒深睡眠，均以 RTC 定时器唤醒 | `main/demo_low_power.c` | 两种模式都强制暂停 ES8311 并回读校验；浅睡眠恢复音频，深睡眠则先暂停 CW2017、释放 I2S/共享 I2C 引脚、休眠并保持 LCD 引脚，唤醒时重启应用；当前 demo 只提供 RTC 定时器唤醒 |
| 共享总线 | ES8311 与 CW2017 共用 I2C0 | `bsp_i2c_*` | 所有设备复用 BSP 持有的总线；不能为扫描或新设备再创建同端口总线 |
| 日志与烧录 | ESP32-C3 原生 USB Serial/JTAG | ESP-IDF console | GPIO18/19 保留给 USB；UART0 默认 TX GPIO21 与背光冲突 |

所有引脚、地址、面板参数和按键电压窗口只在 [`components/bsp/include/bsp_pins.h`](../components/bsp/include/bsp_pins.h) 定义。应用代码不得复制这些常量。完整引脚表、面板初始化、ADC 阈值、I2C 地址规则、音频时钟和内存说明见 [AI 硬件开发指南](hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md)。

应用也可以使用 ESP-IDF 提供的定时器、FreeRTOS 任务和内部 Flash/NVS；番茄钟分支提供了 NVS 示例。Wi-Fi 和 Bluetooth LE 仍是 ESP-IDF 应用服务而非 BSP API：其菜单页面仅在打开时初始化对应协议栈、退出时释放。`demo/claude-buddy-port` 仍是更完整的 BLE 应用架构参考，不能替代对当前板卡天线、射频表现、功耗和共存行为的实测。

### 不属于当前能力契约的事项

公开固件能力以表中接口为限，不能仅凭 ESP32-C3 芯片能力推断其他板级接口。新增硬件接口必须提供明确的 BSP 定义和实机验收标准。

</details>

## 项目结构

板级支持放在 `components/bsp`；应用页面、状态与任务放在 `main`。
开发自定义固件时继续保持这条边界。

<details>
<summary><strong>展开仓库目录说明</strong></summary>

```text
components/bsp/include/  BSP 公开 API 与 bsp_pins.h 硬件事实
components/bsp/src/      显示、按键、音频、电池、共享 I2C 实现
main/                    最小菜单、LVGL UI 与独立硬件演示页
tests/                   可脱离硬件运行的轻量逻辑测试源
tools/                   本地与 CI 共用的验证及固件校验脚本
docs/                    项目说明、变更记录、工程/协作规范与设计参考
.github/                 GitHub 社区文档、PR 模板、Issue Form 与 CI 工作流
sdkconfig.defaults       ESP32-C3、USB console、Flash、LVGL 默认配置
partitions.csv           最简默认分区：NVS、PHY data 和单个 factory 应用
dependencies.lock        可复现的 ESP-IDF Managed Component 解析结果
AGENTS.md                AI agent 必读入口（与 AGENTS.zh_CN.md 配对）
CLAUDE.md                Claude Code 指向 AGENTS.md 的入口（含中文配对）
LICENSE                  仓库许可证
```

</details>

## 文档索引

工程与协作文档定义开发规则，示例与档案提供参考资料。按当前任务选择入口即可。

| 入口 | 你可以找到 |
| --- | --- |
| [开发指南](development/README.zh_CN.md) | AI 工作流、工程规范、CI 与发布流程 |
| [AI 技能](../skills/README.zh_CN.md) | 开发、环境准备、构建、真机测试与故障诊断 |
| [硬件资料](hardware-design/README.zh_CN.md) | 板卡事实、接口边界、验收清单与排障 |
| [中文字体](development/engineering/lvgl-chinese-fonts.zh_CN.md) | 字形覆盖、控件字体选择，以及中文空白排查 |
| [Wi-Fi 配网](development/engineering/wifi-provisioning.zh_CN.md) | 蓝牙配网实现参考与配套小程序 |
| [社区作品与经验](reference/README.zh_CN.md) | `docs/reference/<username>/` 下的应用档案和可复用知识 |
| [参与贡献](contribution/README.zh_CN.md) | 文档、提交与 Pull Request 约定 |
| [品牌素材](brand/README.zh_CN.md) | 产品视觉参考与[品牌说明](brand/brand-and-product.zh_CN.md) |
| [Fork 指南](fork-guide.zh_CN.md) · [更新记录](CHANGELOG.zh_CN.md) | 下游工作流与版本历史 |

---

[参与贡献](../.github/CONTRIBUTING.zh_CN.md) · [获取帮助](../.github/SUPPORT.zh_CN.md) · [行为准则](../.github/CODE_OF_CONDUCT.zh_CN.md) · [安全说明](../.github/SECURITY.zh_CN.md) · [MIT 许可证](../LICENSE)

AI 助手请从 [`AGENTS.md`](../AGENTS.md) 开始，再按任务路由读取相关文档。
