<p align="right">
  <strong>简体中文</strong> · <a href="readme-update.md">English</a>
</p>

# 动作 E：更新根 README

本动作更新 fork 在相关分支上的根 `README.md`，反映新发布或归档的应用。它是[项目开发完成流程](../project-completion.md)列出的六项可选动作之一。

根 README 路径特意留给 fork 所有者。上游的项目概览位于 `docs/README.md`；fork 可以添加自己的根 README 来解释其产品，而不替换上游文档。

fork 的 `main` 与上游保持同步，产品工作放在 `feature/*` 分支上，因此根 README 会存在于多个分支。每个分支的根 README **各自处理**——`main` 的 README 和某个 `feature/*` 分支的 README 是两个独立决定。

## 何时建议

README 更新与其它五项一样是**可选**动作，也是归档的默认伴随动作：当应用归档到 `plays/`（动作 D）时，README 同步会作为该动作的一部分执行。归档本身是可选——开发者可以拒绝——但每当项目完成时，都应在宿主分支和 fork `main` 上刷新 README，让应用在其开发处被登记。

## 规则

- 只动 fork 所有的根 README（`README.md` / `README.zh_CN.md`）；不修改 `docs/README.md` 的上游项目概览。
- 检查每个相关分支（`main` 和当前 `feature/*` 分支）上的根 README，而不是只看一个分支。
- fork `main` 根 README 是 **fork 项目目录**：它**完整包含**每个项目自己 README 的内容——一个完整描述应用做什么及如何使用的说明（交互、模式、按键、持久化与备注），而不是一行简介再跟一个分支链接。内容从宿主分支的 README 拉取。
- fork 根 README 与宿主分支的根 README 都是 fork 独有内容。**直接提交（merge）**，不要另开 PR；只有当变更要进上游时才开 PR。
- 遵循仓库语言规则：默认 `.md` 用英文，配对的 `.zh_CN.md` 用简体中文，同一变更内对齐。

## 步骤

1. 确认同意与可用的 GitHub 通道（GitHub MCP、GitHub skill 或 `gh`）。
2. 在宿主 `feature/*` 分支上：缺 README 则补齐双语 README 对，已经有则更新它，添加或刷新应用自己的描述。
3. 在 fork `main` 上：更新根 README 对，让发布的该应用能从仓库落地页找到，并**完整包含宿主分支 README 的内容**。
4. 直接把 README 更新提交到分支 / fork `main`（fork 独有内容）；除非是上游变更，否则不为它开 PR。

## 相关文档

- Fork 工作流与根 README 归属：[fork-guide.md](../../fork-guide.md)
- 应用归档 skill：[plays-archive](../../../skills/plays-archive/SKILL.md)
- 文档规范：[doc-conventions.md](../../contribution/doc-conventions.md)
