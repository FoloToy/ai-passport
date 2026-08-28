<p align="right">
  <strong>简体中文</strong> · <a href="archive-plays.md">English</a>
</p>

# 动作 D：归档应用到 plays

本动作把已发布的应用归档到上游 `plays/` 应用档案，这样它能在仓库内留存、便于后续查询。它是[项目开发完成流程](../project-completion.md)列出的六项可选动作之一。

工作流由 `plays-archive` skill 驱动。

## 输入

- 应用名（lowercase-kebab-case）。
- [共享发布属性](../project-completion.md#共享发布属性)：双语标题与简介、源码地址。

## 步骤

1. 确认同意与可用的 GitHub 通道（GitHub MCP、GitHub skill 或 `gh`）。
2. 在 `plays/<username>/<app-name>/` 下生成双语 AI 功能总结（`README.md` / `.zh_CN.md`），有根 README 时合并它。
3. 记录发布元数据——双语标题与简介、源码地址，其中包括封面按文件名与格式记录；但不提交封面图本身。档案为纯文本（text-only）。
4. 各自处理每个分支的根 README（必需的 README 同步见 [readme-update.md](./readme-update.md)）。
5. 只在专门分支上提交总结；不存固件 `.bin`，也不存封面图。
6. 经审查后，向上游项目开归档 PR。

## 安全

- 绝不在档案中保存合并固件 `.bin`，也不保存封面图；档案为纯文本，两者都是构建/发布产物。
- 未经开发者审查与同意，不提交。

## 相关文档

- 应用档案约定：[plays/README.md](../../../plays/README.md)
- Skill：[plays-archive](../../../skills/plays-archive/SKILL.md)
- README 更新（必须）：[readme-update.md](./readme-update.md)
