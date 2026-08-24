# AGENTS.md

给 AI agent（Claude Code / Codex / Cursor / Cline 等）读的仓库说明。Claude Code 另见 `CLAUDE.md`（薄引用本文件）。

**先读根目录所有 README**：开始工作前，阅读仓库根目录下的**所有** `README*` 文件——包括上游的 `README.en_US.md`、`README.zh_CN.md`，以及 fork 用户可能自行创建的说明文件（如 `README.md` 或自定义 README）。这些文件是重要的 AI 参考（含项目定位、硬件事实、使用约定等），AI 不应只依赖本文件。

**文档生成规范**：本仓库的说明文档（`AGENTS.md`、`CLAUDE.md`、`CHANGELOG.md`、`docs/**`、`assets/**`、`skills/**` 等）均为 AI 生成。请**勿手动输入或修改**这些文档内容——需要更新时，通过 AI agent 按本项目规范重新生成/改写，保持文档与代码、与约定一致；手动改动会造成不一致或被后续流程覆盖。

**文档分类规范**：文档分为两类，提交到上游的 PR 里**只允许包含公共文档**：

- **公共文档**：适用于**任何项目**的通用规范/约定（如 AI 工作流、commit 风格、文档生成规范、文档分类规范、目录骨架约定、CI/构建通用做法等）。这些**可以**提交到上游项目。
- **特定项目文档**：只适用于**本项目**的专属内容（如本项目的硬件事实、BSP 接口、专属索引/说明、`docs/hardware-design` 的硬件指南等）。这些**不得**提交到上游项目。

判断原则：某段内容去掉项目名/硬件/专属细节后仍对任意项目成立，就是公共文档；否则是特定项目文档。提交上游前，只保留公共文档部分，剔除非公共内容。

## 项目概述

本仓库是 **FoloToy AI Passport** 的开源硬件开发基线。它是一个面向 AI agent 的开放式可穿戴 AI 硬件：`main` 是最小但完整的可运行基线，集中存放**已确认的硬件事实、稳定接口、资源边界、参考实现与验收方法**，AI agent 可据此识别能力与限制、实现并构建应用。本仓库常被 fork 后二次开发，fork 用户的使用约定见下文「给 fork 用户」。

- **平台**：ESP32-C3（32 位 RISC-V，无 PSRAM，8MB Flash），ESP-IDF 5.5.x（已知开发环境 5.5.3）。
- **屏**：ST7789P3，240×320 竖屏 RGB565，SPI2 40MHz，LVGL。
- **交互**：`UP`/`DOWN`/`OK` 三键（GPIO0 ADC 电阻梯）；音频 ES8311（I2S0 全双工）；电池 CW2017；I2C0 共享总线。
- **协议**：MIT LICENCE，仓库根许可证文件为准。

## 目录结构

```
docs/
  software-design/README.md   软件设计文档子目录骨架
  hardware-design/
    README.md                 硬件设计文档子目录骨架
    AI_HARDWARE_DEVELOPMENT_GUIDE.md   完整硬件开发指南与排障参考（上游）
components/bsp/               板级支持包：显示、按键、音频、电池、I2C（稳定 API 与 bsp_pins.h 硬件事实）
main/                         设备固件应用：LVGL 菜单 + 独立 demo_*.c 硬件验证页（新 demo 需实现 demo.h 声明的 enter/exit/key 接口）
assets/                       预置资源：fonts/ images/ music/（各含 README.md）
skills/                       可复用技能目录（每个 skill 独立子目录）
tests/                        轻量级逻辑测试（无硬件可运行）
sdkconfig.defaults            ESP32-C3、USB console、Flash、LVGL 默认配置
README.en_US.md               上游英文说明（FoloToy AI Passport）
README.zh_CN.md               上游中文说明（FoloToy AI Passport）
```

**强约束**：`main` 始终与上游 `FoloToy/ai-passport` 的 `main` 保持同步（最新基线），不承载 fork 特有功能改动；fork 特有的固件功能都在各自功能分支（`feature/*`）开发。

**给 fork 用户**：fork 后，`main` 分支**只允许增加/修改根目录的 `README.md`**（可建自己的说明/README 变体），**不允许改其它任何文件**——这样 fork 的 `main` 与上游保持最新同步、不产生冲突。任何其它修改（固件功能、文档规整等）一律在 `feature/*` 分支进行，用 PR 合并。

## 构建与验证

使用 ESP-IDF 5.5.x（已知开发环境 5.5.3）：

```bash
get_idf553                    # 进入仓库的 ESP-IDF 5.5.3 环境
idf.py set-target esp32c3     # 配置目标芯片（fresh checkout 后/换 target 后运行）
idf.py build                  # 编译固件，验证依赖
idf.py flash monitor          # 烧录并打开日志
idf.py fullclean              # 配置过期时清空生成状态（勿用于清理用户源码改动）
```

当前基线含一个可独立运行的纯逻辑测试：

```bash
cc -std=c11 -Wall -Wextra -Werror -Imain \
  tests/test_ui_pixel_math.c main/ui_pixel_math.c \
  -o /tmp/test_ui_pixel_math
/tmp/test_ui_pixel_math
```

改动后至少跑 `idf.py build`（最小自动化检查）+ 适用逻辑测试；涉及物理外设的改动必须在真机运行 README 验收清单，并把“编译通过”与“硬件验证通过”分开记录（禁止把编译通过描述成硬件验证通过）。

## 代码约定

- **语言/风格**：C 用四空格缩进 + K&R 大括号，跟随相邻文件；函数/局部变量 `snake_case`，公开硬件常量 `BSP_*`，文件内状态 `s_` 前缀；BSP API 用 `bsp_` 前缀，demo 入口 `demo_<feature>_<action>`；内部符号优先 `static`。UI 文案用英文，解释性注释可用中文；保留记录硬件寄存器值与内存约束的注释。
- **复用既有组件**：可复用硬件逻辑放 `components/bsp`；菜单/动画/业务交互/验证页面放 `main`。不要另造轮子，遵循既有 BSP API。
- **注释职责**：对不自解释的函数、接口、结构体字段、请求参数和关键返回值补简洁注释，说明职责、语义和使用边界（含阻塞行为、线程上下文、内存所有权、失败值、初始化顺序）。
- **测试同步**：写代码时同步新增测试用例，或修改受本次改动影响的既有测试；若暂无合适自动化测试落点，在项目规范里写清测试缺口和手工验证路径。
- **Redis TTL**：本固件**不涉及 Redis**（N/A），无需 TTL 约束；若未来引入缓存组件，按“默认设过期时间、长期缓存需说明原因/风险/清理机制”执行。
- **资源约束**：ESP32-C3 无 PSRAM，不增加 LVGL buffer / 音频分配 / 任务栈前先核对内部 RAM；足够的总空闲堆不保证有足够大连续块。

## 提交规范

- commit 标题：`type(scope): 简述`，`type` ∈ `feat/fix/docs/refactor/perf/test/chore/build/ci`，简述祈使句、≤50 字符、结尾不加句号、默认中文（技术术语保留英文）。例如 `feat(bsp): ...`、`docs: ...`。
- 一个 commit 只做一件事，message 描述最终 diff，不叙述调试过程。
- commit 后同一轮内 `git push`（协作仓库走分支/PR，本 fork 功能开发在 `feature/*`）。
- push 前检查仓库根是否同时存在 `AGENTS.md` 和 `CLAUDE.md`，缺失时先补齐。
- 任何实际文件变更都同步记录到 `CHANGELOG.md`；项目没有该文件时先在仓库根创建。
- **最终需求回写**：需求沟通形成的最终需求，不要只留在 memory；必须同步写回本文件、需求文档或项目内对应规范文件。其他记录了本项目决策、约定、踩坑、架构边界、运行方式、测试方式、发布流程或团队口径的 memory，同样要回写。

## 提交与 PR 约定

- PR 应说明测试的硬件/版本、行为变更摘要、构建与真机结果；显示类改动附照片/截图；链接相关 issue，并说明接线、引脚映射或兼容性问题。
- 对引脚、显示旋转、codec 时钟、ADC、DMA 改动，必须在 PR 里显式记录观察到的真机结果。
