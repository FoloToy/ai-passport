<p align="right">
  <strong>简体中文</strong> · <a href="experience.md">English</a>
</p>

# 动作 C：发布经验

本动作把一次发布中可复用的、持久的开发经验固化，并作为文档 PR 提交到上游项目。它是[项目开发完成流程](../project-completion.md)列出的六项可选动作之一。

工作流由 `experience-pr` skill 驱动。

## 聚焦

采集 fork 自身相对上游 `docs/` 的差异——开发者在这个 fork 上创建或修改过的文档。只提取持久、可复用的经验：

- fork 记录了哪些上游没有的东西，以及为什么。
- 硬件事实、接口、时序、资源预算或失败行为。
- 构建、校验或发布流程上的改进。
- 能适用于下一次发布的一般化经验。

## 分流

提交前先决定每条经验归属：

- **通用经验回上游**——对任何用户都有益、属于上游基线的经验。作为 PR 提交到上游项目。
- **fork 私有定制留在 fork**——产品定制内容、fork 私有业务规则或专有资产。不提交上游，记录在本地。

## 步骤

1. 确认同意与可用的 GitHub 通道（GitHub MCP、GitHub skill 或 `gh`）。
2. 对比 fork 与上游，找出 `docs/` 的差异。
3. 提取并分流可复用经验。
4. 在 `docs/experiences/<username>/` 下写入一个条目（一个 `.md` 文件，配 `.zh_CN.md`），按内容摘要命名（lowercase-kebab-case），并从经验索引链接它。
5. 把变更交给开发者审查，然后在获得明确批准后再 commit、push 到 fork、并向上游开 PR。

## 相关文档

- 经验索引：[experience-notes.md](../experience-notes.md)
- Skill：[experience-pr](../../../skills/experience-pr/SKILL.md)
- Fork 工作流：[fork-guide.md](../../fork-guide.md)
