# AGENTS.md

给 AI agent（Claude Code / Codex / Cursor / Cline 等）读的仓库说明。Claude Code 另见 `CLAUDE.md`（薄引用本文件）。

本文件是仓库权威 AI 规范的**入口与索引**，具体规则已按主题拆分到 `docs/` 下公共文档。规则有更新时改对应 `docs/` 文件，不要在本文件另起一套，以免两份文档 drift。

## 项目概述

本仓库是 **FoloToy AI Passport** 的开源硬件开发基线。它是一个面向 AI agent 的开放式可穿戴 AI 硬件：`main` 是最小但完整的可运行基线，集中存放**已确认的硬件事实、稳定接口、资源边界、参考实现与验收方法**，AI agent 可据此识别能力与限制、实现并构建应用。本仓库常被 fork 后二次开发，fork 用户的使用约定见 [docs/fork-guide.md](docs/fork-guide.md)。

- **平台**：ESP32-C3（32 位 RISC-V，无 PSRAM，8MB Flash），ESP-IDF 5.5.x（已知开发环境 5.5.3）。
- **屏**：ST7789P3，240×320 竖屏 RGB565，SPI2 40MHz，LVGL。
- **交互**：`UP`/`DOWN`/`OK` 三键（GPIO0 ADC 电阻梯）；音频 ES8311（I2S0 全双工）；电池 CW2017；I2C0 共享总线。
- **协议**：MIT LICENCE，仓库根许可证文件为准。

## 必读文档（按顺序阅读）

1. [docs/contribution/doc-conventions.md](docs/contribution/doc-conventions.md) —— **最先读**：先读根目录所有 README、文档生成规范、文档分类规范（公共/特定项目）。
2. [docs/fork-guide.md](docs/fork-guide.md) —— 目录结构、fork 工作流、`main` 保持干净的原因与例外、`assets/docs` 使用约定。
3. [docs/development/build-and-test.md](docs/development/build-and-test.md) —— 构建与验证命令、改动验证要求。
4. [docs/development/coding-conventions.md](docs/development/coding-conventions.md) —— 代码约定（语言风格、复用、注释、测试同步、资源约束等）。
5. [docs/contribution/commit-and-pr.md](docs/contribution/commit-and-pr.md) —— 提交规范与 PR 约定。
