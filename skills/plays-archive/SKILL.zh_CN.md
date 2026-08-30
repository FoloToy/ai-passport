---
name: plays-archive
description: 固件发布之后，把已发布的应用归档到上游 FoloToy ai-passport 仓库的 plays/ 目录，附一份 AI 生成的双语功能说明（纯文本；封面图只记录文件名与格式，不提交）。
---

<p align="right">
  <strong>简体中文</strong> · <a href="SKILL.md">English</a>
</p>

# 把应用归档到 plays

本 skill 把已发布的应用归档到上游 `FoloToy/ai-passport` 仓库的 `plays/` 应用档案库，让它在
仓库内可被检索、便于后续查询。它只在固件发布后（发布流程见
`docs/development/publish-to-community.md`）运行，且仅在开发者要求归档该应用时执行。

## 安全与同意门槛（必须先做）

在创建、写入或提交任何内容之前，必须先满足以下所有门槛。

1. **先确认同意。** 本工作涉及项目私有内容。先向开发者确认是否同意归档该应用；开发者拒绝则
   立即停止。
2. **绝不在当前分支上修改或提交。** 以最新上游 `main` 为干净基线，另起一个独立分支或 worktree
   承载，推到开发者的 fork（`origin`），并从该 fork 分支向上游 `FoloToy/ai-passport` 开 PR。
   保持当前 checkout 不被改动。
3. **不写入凭证或私有数据。** 永远不包含凭证、设备 QR 密钥、私密设备链接、个人数据或未脱敏
   日志。提交任何内容前先运行 `python3 tools/check_repo.py`。

## 确定要归档的内容

确认应用名、其所属源码（例如某个 `demo/*` 分支或 `main/`），以及贡献者的 GitHub 用户名。
用小写连字符的用户名和应用名作为两级路径：`plays/<username>/<app-name>/`。
完整约定见 [`../../plays/README.md`](../../plays/README.md)。

## 检查项目 README

生成功能说明前，先检查 `main` 分支和当前分支**根目录**的 README：

- `git ls-tree --name-only main README.md` —— `main` 分支有没有 README？
- `test -f README.md` —— 当前分支有没有 README？

遵循仓库规则：根 README 路径保留给 fork owner（见 `docs/fork-guide.md`）；除非 fork 确实拥有
根 README，否则不要创建。

1. **若有 README**（`main` 或当前分支有）：归档时把 **README 内容合并进功能说明**，让说明既
   反映人类可读的描述，也反映代码。已存在的 README 归它所属的分支保留。
2. **若没有 README**：直接从实现总结，不合并 README。
3. **归档完成后**，对每个分支的根 README **各自处理**（不是一个合并判断）：
   - 对**没有**根 README 的分支：在**该分支**创建（或更新）README，让归档的应用能从 fork 自己的
     README 检索到。
   - 对已经**有**根 README 的分支：**提示开发者更新它**，以反映新归档的应用。

## 生成功能说明

先收集开发者发布到社区时填写的元数据（双语标题、双语描述，以及他们提交的源码地址），然后写
`plays/<username>/<app-name>/README.md` 及其配对 `.zh_CN.md`，作为为后续查询而生成的 AI 功能说明
（不是发布产物）。记录：

- **发布标题与描述**：发布到社区时开发者提交的双语标题、双语描述。
- 应用名与一句话定位。
- 应用做什么、功能清单。
- 交互与玩法（按键、屏幕、流程）。
- 应用来源，用**开发者发布时提交的源码地址**（HTTPS Git 源码页）精确定位。
- 封面图文件名与格式，仅作为发布元数据记录——封面图本身**不**提交（档案为纯文本）。

若根 README 存在，把它合并进说明，而不是忽略人类可读的描述。

默认 `.md` 用英文、配对 `.zh_CN.md` 用简体中文，并在同一次变更中对齐。

## 封面图

档案为**纯文本**：**不要**提交封面图，只在功能说明里记录其文件名与格式作为发布元数据。图片本身随
社区发布留存。若需为发布（而非归档）生成封面，参考
[`docs/assets/brand/`](../../docs/assets/brand/README.md) 下的官方产品图：生成时必须传一张参考图
（如 `ai-passport-front.png` 或某款配色外壳渲染图）作为生成调用输入，保留其外壳、按键、接口与
钥匙扣孔原样，只把参考图的屏幕区域**重绘**成该玩法的真实屏显内容，屏幕的尺寸、比例、圆角与外壳内
位置与参考保持一致。完整约定见 [`docs/assets/brand/README.md`](../../docs/assets/brand/README.md)。

## 提交

在独立分支上提交总结（英文祈使句 Conventional Commit 标题，例如
`docs(plays): add <app-name> application archive`）。若创建或更新了根 README，一并纳入同一次变更。
**不要**在这里存合并固件 `.bin`；它是构建/发布产物。按 Build、Host tests、Device tests、
Unverified 分别上报。

审查后，通过第一个可用的 GitHub 通道（GitHub MCP、GitHub skill、或
`gh pr create --repo FoloToy/ai-passport --base main --head <fork>:<branch>`）从 fork 分支
向上游 `FoloToy/ai-passport` 开 PR，并回读确认。开 PR 需要单独的再次确认。

## 本 skill 不做的事

- 不发布固件、不运行 publisher 流程。
- 不改生产源码、不改固件。
- 不存储固件 `.bin` 二进制。
- 不存储封面图（档案为纯文本）。
- 未经开发者审查与同意，不自动提交任何内容。

## 相关文档

- 应用档案约定：`../plays/README.md`
- 发布后收尾总览：`docs/development/project-completion.md`
- 固件发布：`docs/development/publish-to-community.md`
- 贡献与提交规则：`docs/contribution/commit-and-pr.md`
