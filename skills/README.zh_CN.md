<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 技能目录（Skills）

本目录用于保存项目可能用到的 skill（面向 AI agent 的可复用技能/工作流说明）。**每个 skill 必须按独立目录保存**，便于检索与维护。

## 目录约定

- 每个 skill 一个子目录，目录名即 skill 名称，命名简短、语义明确。
- 每个 skill 目录内至少有一个说明文件（建议 `SKILL.md`），顶部用 YAML frontmatter 标注 `name` 与 `description`，其中 `description` 作为触发指纹，说清「何时触发、做什么」。
- 复杂的 skill 可在其目录下增加 `references/`（长文档）、`scripts/`（可执行脚本）、`assets/`（模板/样例），与主文件分开放。
- 文档应为纯 markdown，不含二进制内容。

## 如何添加一个 skill

1. 在 `skills/` 下新建以 skill 名命名的目录。
2. 目录内新建 `SKILL.md`，顶部写 `name` + `description` frontmatter。
3. 视需要增加 `references/`、`scripts/`、`assets/` 子目录。
4. 在本 `README.md` 的索引表中登记该 skill 的名称与一句话说明。

## 现有技能索引

| 技能 | 功能 |
| --- | --- |
| [issue-suggestions](issue-suggestions/SKILL.md) | 发布后，收集开发者的改进点，整理成提交到上游的功能建议 issue。 |
| [experience-pr](experience-pr/SKILL.md) | 发布后，收集可复用的开发经验，并作为文档 PR 提交。 |
| [plays-archive](plays-archive/SKILL.md) | 发布后，把已发布应用归档到上游 `plays/`，附 AI 生成的双语说明与封面图。 |
