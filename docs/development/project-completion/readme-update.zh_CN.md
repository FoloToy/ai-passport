<p align="right">
  <strong>简体中文</strong> · <a href="readme-update.md">English</a>
</p>

# 动作 E：更新根 README

本动作更新 fork 在相关分支上的根 `README.md`，反映新发布或归档的应用。它是[项目开发完成流程](../project-completion.md)列出的六项可选动作之一。

根 README 路径特意留给 fork 所有者。上游的项目概览位于 `docs/README.md`；fork 可以添加自己的根 README 来解释其产品，而不替换上游文档。

fork 的 `main` 与上游保持同步，产品工作放在 `feature/*` 分支上，因此根 README 会存在于多个分支。每个分支的根 README **各自处理**——`main` 的 README 和某个 `feature/*` 分支的 README 是两个独立决定。

## 规则

- 只动 fork 所有的根 README（`README.md` / `README.zh_CN.md`）；不修改 `docs/README.md` 的上游项目概览。
- 检查每个相关分支（`main` 和当前 `feature/*` 分支）上的根 README，而不是只看一个分支。
- 遵循仓库语言规则：默认 `.md` 用英文，配对的 `.zh_CN.md` 用简体中文，同一变更内对齐。

## 步骤

1. 确认同意与可用的 GitHub 通道（GitHub MCP、GitHub skill 或 `gh`）。
2. 对每个相关分支，检查是否存在 fork 所有的根 README。
3. 若存在，更新它，纳入新归档的应用。
4. 若不存在，创建一份 fork 所有的根 README 描述产品。

## 相关文档

- Fork 工作流与根 README 归属：[fork-guide.md](../../fork-guide.md)
- 应用归档 skill：[plays-archive](../../../skills/plays-archive/SKILL.md)
- 文档规范：[doc-conventions.md](../../contribution/doc-conventions.md)
