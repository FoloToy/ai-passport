---
name: plays-archive
description: 固件发布之后，把已发布的应用归档到本仓库的 plays/ 目录，附一份 AI 生成的双语功能说明与封面图。
---

<p align="right">
  <strong>简体中文</strong> · <a href="SKILL.md">English</a>
</p>

# 把应用归档到 plays

本 skill 把已发布的应用归档到仓库的 `plays/` 应用档案库，让它在仓库内可被检索、便于后续查询。
它只在固件发布后（发布流程见 `docs/development/publish-to-community.md`）运行，且仅在开发者
要求归档该应用时执行。

## 安全与同意门槛（必须先做）

在创建、写入或提交任何内容之前，必须先满足以下所有门槛。

1. **先确认同意。** 本工作涉及项目私有内容。先向开发者确认是否同意归档该应用；开发者拒绝则
   立即停止。
2. **绝不在当前分支上修改或提交。** 以最新上游 `main` 为干净基线，另起一个独立分支或 worktree
   承载该变更。保持当前 checkout 不被改动。
3. **不写入凭证或私有数据。** 永远不包含凭证、设备 QR 密钥、私密设备链接、个人数据或未脱敏
   日志。提交任何内容前先运行 `python3 tools/check_repo.py`。

## 确定要归档的内容

确认应用名及其所属源码（例如某个 `demo/*` 分支或 `main/`）。用小写连字符的应用名作为子目录名。
完整约定见 [`../../plays/README.md`](../../plays/README.md)。

## 生成功能说明

写 `plays/<app-name>/README.md` 及其配对 `.zh_CN.md`，作为为后续查询而生成的 AI 功能说明
（不是发布产物）。记录：

- 应用名与一句话定位。
- 应用做什么、功能清单。
- 交互与玩法（按键、屏幕、流程）。
- 所属源码分支或目录。
- 封面图文件名与格式。

默认 `.md` 用英文、配对 `.zh_CN.md` 用简体中文，并在同一次变更中对齐。

## 添加封面图

封面放在 `plays/<app-name>/<app-name>-cover.<webp|png|jpg>`，commit 进仓库。选有代表性且小于
10 MiB 的图。

## 提交

在独立分支上提交总结与封面（英文祈使句 Conventional Commit 标题，例如
`docs(plays): add <app-name> application archive`）。**不要**在这里存合并固件 `.bin`；它是构建/
发布产物。按 Build、Host tests、Device tests、Unverified 分别上报。

## 本 skill 不做的事

- 不发布固件、不运行 publisher 流程。
- 不开代码 PR、不改生产源码。
- 不存储固件 `.bin` 二进制。
- 未经开发者审查与同意，不自动提交任何内容。

## 相关文档

- 应用档案约定：`../plays/README.md`
- 发布后收尾总览：`docs/development/after-release.md`
- 固件发布：`docs/development/publish-to-community.md`
- 贡献与提交规则：`docs/contribution/commit-and-pr.md`
