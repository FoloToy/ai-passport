<p align="right">
  <strong>简体中文</strong> · <a href="fork-guide.md">English</a>
</p>

# Fork 工作流（Fork Guide）

## 目录结构

```
docs/
  software-design/README.md   软件设计文档子目录骨架
  hardware-design/
    README.md                 硬件设计文档子目录骨架
    AI_HARDWARE_DEVELOPMENT_GUIDE.md   完整硬件开发指南与排障参考（上游）
  contribution/               协作规范（doc-conventions.md、commit-and-pr.md，见 README.md 索引）
  development/                工程规范与 AI 开发工作流（build-and-test.md、coding-conventions.md、agent-guide.md，见 README.md 索引）
  README.md                   上游英文说明（FoloToy AI Passport；GitHub 主 README）
  README.zh_CN.md             上游中文说明（FoloToy AI Passport）
  INDEX.md                    仓库根总索引（各目录索引表格 + 根治理文档）
  assets/                     fork 补充文档素材目录（README 不足以说明项目时存放补充文档与素材；上游 main 只保留空目录 `.gitkeep`，内容文件仅存在于 fork）
  fork-guide.md               本文档：fork 工作流与约定
components/bsp/               板级支持包：显示、按键、音频、电池、I2C（稳定 API 与 bsp_pins.h 硬件事实）
main/                         设备固件应用：LVGL 菜单 + 独立 demo_*.c 硬件验证页（新 demo 需实现 demo.h 声明的 enter/exit/key 接口）
assets/                       预置资源：fonts/ images/ music/（各含 README.md）
skills/                       可复用技能目录（每个 skill 独立子目录）
tests/                        轻量级逻辑测试（无硬件可运行）
sdkconfig.defaults            ESP32-C3、USB console、Flash、LVGL 默认配置
```

**强约束**：`main` 始终与上游 `FoloToy/ai-passport` 的 `main` 保持同步（最新基线），不承载 fork 特有功能改动；fork 特有的固件功能都在各自功能分支（`feature/*`）开发。

## 为什么 `main` 保持干净

有两个原因——(1) **随时与上游保持一致、无冲突**，给多任务并行开发提供最新基线；(2) **默认用户会开发很多小项目**，为了便于整理，通过分支（`feature/*`）保存每个项目，`main` 只作为同步基线不混入具体项目改动。

**例外**：如果用户**执意要在 `main` 分支直接开发**，需要**停用/关闭 CI 的自动同步任务**（见 `.github/workflows/sync-main.yml`，说明见 [docs/development/CI-sync-main.md](development/CI-sync-main.md)），否则自动同步会把上游改动强行合入，产生冲突或覆盖本地开发内容。

**fork 用户注意（整个 CI 通用）**：fork 默认的 GitHub Actions 是**关闭**的，需要**手动打开启用**（GitHub 仓库 Actions 页面 → 找到需要的 workflow，如 CI 自动同步 → Enable）后，对应的 CI 才会生效。

## 给 fork 用户

fork 后，`main` 分支**只允许增加/修改根目录的 `README.md` 和 `docs/assets/` 目录**（可建自己的说明/README 变体），**不允许改其它任何文件**——这样 fork 的 `main` 与上游保持最新同步、不产生冲突。任何其它修改（固件功能、文档规整等）一律在 `feature/*` 分支进行，用 PR 合并。

**新开项目开发前**：从**与上游 `FoloToy/ai-passport:main` 保持一致**的基线创建 `feature/*` 分支——即以**上游最新 `main`** 作为分叉点（例如 fetch 上游后基于 `upstream/main` 开分支），而**不要**基于 fork 自己的 `main`。因为 fork 的 `main` 可能过期、或因为各种原因无法成功同步。这样每个新项目都从当前上游基线出发，避免基于过期/分歧的 fork `main`。

### 为什么根目录不放置 README

**根目录的 `README.md` 是预留给开发者自行放置的，因此上游留空**：

- 上游 main 的根目录**故意不放 README**，把这一位置预留给 fork 开发者。上游项目说明位于 `docs/README.md`（GitHub 从 docs/ 识别主 README），不占用根目录。
- 开发者 fork 项目后，可将**自己的内容**写到根目录的 `README.md`，以介绍 fork 后的项目——例如改动来源、二次开发说明、自定义用法等。这样 fork 的根目录 README 与上游互不冲突，也无需覆盖上游文档。

> **GitHub 显示行为**：GitHub 显示优先级为根目录 `README.md` > `docs/README.md`，因此 fork 用户在根目录自建 `README.md` 后即覆盖 `docs/README.md` 的显示——正契合上面"根目录 README 预留给开发者"的设计意图。

## docs/assets 使用约定

个人项目可能 `README.md` 不足以完整说明项目，`docs/assets/` 正是用于在 fork 下保存 README 的**补充文档与素材**（架构说明、产品设计、图片素材等）。`docs/assets/` 在**上游 main 上必须为空**（仅保留 `.gitkeep` 占位），因此 fork 用户应在该目录下自行创建 `readme.md` 说明使用方式。建议 `docs/assets/readme.md` 包含：

- 本目录用途：README 的补充文档与素材所在。
- 目录结构约定：如何组织文档与素材子目录。
- AI 使用方法：进入仓库时如何结合上游 `docs/README.md`（或 fork 自建根 README）与本目录补充信息理解项目。
- 边界：该目录仅存在于 fork，内容不得同步回上游 main。

## 文档与经验的分区

文档与经验遵循同样的分流原则。fork 专属产品定制（架构说明、产品设计、fork 专属素材）留在
fork，放在 `docs/assets/` 下，不提交回上游。通用、上游也受益的文档或经验改进——持续的硬件事实、
可复用的接口、能帮助任何 AI Passport 用户的构建或发布流程改进——作为 PR 提交回上游。
`plays/` 应用档案与发布后的经验沉淀属于上游，作为 PR 提案回上游。发布后的工作请用
`experience-pr` 与 `plays-archive` skill，见 `docs/development/project-completion.md`。
