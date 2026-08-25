# 提交与 PR 规范（Commit & PR）

> 定位：公共文档，适用于任何项目，可提上游。

## 提交规范

- commit 标题：`type(scope): 简述`，`type` ∈ `feat/fix/docs/refactor/perf/test/chore/build/ci`，简述祈使句、≤50 字符、结尾不加句号、默认中文（技术术语保留英文）。例如 `feat(bsp): ...`、`docs: ...`。
- 一个 commit 只做一件事，message 描述最终 diff，不叙述调试过程。
- commit 后同一轮内 `git push`（协作仓库走分支/PR，本 fork 功能开发在 `feature/*`）。
- push 前检查仓库根是否同时存在 `AGENTS.md` 和 `CLAUDE.md`，缺失时先补齐。
- 任何实际文件变更都同步记录到 `CHANGELOG.md`；项目没有该文件时先在仓库根创建。
- **最终需求回写**：需求沟通形成的最终需求，不要只留在 memory；必须同步写回本文件、需求文档或项目内对应规范文件。其他记录了本项目决策、约定、踩坑、架构边界、运行方式、测试方式、发布流程或团队口径的 memory，同样要回写。

## 提交与 PR 约定

- **PR 标题与 commit 标题同规范**：`type(scope): 简述`，`type` ∈ `feat/fix/docs/refactor/perf/test/chore/build/ci`，简述祈使句、默认中文（技术术语保留英文）。例如 `docs: 规范化文档体系并补充 CI 工作流`。不要用名词短语当标题（如「文档规范化与 CI 工作流补充」）。
- PR 应说明测试的硬件/版本、行为变更摘要、构建与真机结果；显示类改动附照片/截图；链接相关 issue，并说明接线、引脚映射或兼容性问题。
- 对引脚、显示旋转、codec 时钟、ADC、DMA 改动，必须在 PR 里显式记录观察到的真机结果。
