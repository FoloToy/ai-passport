---
name: issue-suggestions
description: 固件发布之后，把发布固件的开发者本人遇到的改进点整理成 issue，提交到上游 FoloToy ai-passport 项目。
---

<p align="right">
  <strong>简体中文</strong> · <a href="SKILL.md">English</a>
</p>

# 收集建议并提交 issue

本 skill 把发布固件的开发者本人的改进点，整理成提交到上游 `FoloToy/ai-passport` 项目的
GitHub issue。它只在固件已发布（发布流程见 `docs/development/publish-to-community.md`）、
且开发者明确要求收集建议时运行。

## 安全与同意门槛（必须先做）

在收集、起草或提交任何内容之前，必须先满足以下所有门槛。这些门槛是最高优先级约束。

1. **先确认同意。** 本工作涉及项目私有内容。开始收集前先向开发者确认是否同意进行发布后的
   收尾工作；开发者拒绝则立即停止。
2. **检查 GitHub 访问通道。** 提交前确认已有可用的 GitHub 通道。按顺序尝试已配置的通道，
   使用第一个可用的：GitHub MCP（已持久且已鉴权）、GitHub skill（如已安装的 `cindy-github`
   能力）、再退到 `gh` CLI（`gh auth status`）。若都不可用，则生成完整 issue 内容供开发者
   手动粘贴，然后停止；绝不替开发者提交。
3. **审查前不得提交。** 先把一切起草好、展示给开发者，等待明确批准。在开发者审查并授权
   之前，不创建、不编辑、不关闭任何 issue。
4. **不写入凭证或私有数据。** 永远不包含凭证、设备 QR 密钥、私密设备链接、个人数据或未脱敏
   日志。

## 收集开发者本人的改进点

收集发布固件的开发者在开发或发布本次版本过程中，遇到的对上游项目有价值的改进点：

- BSP、构建或工具链中的痛点、绕行办法或空白。
- 应在上游文档化或放宽的约束或行为。
- 让下一次发布更顺畅的想法。

**不要**把上游 issue tracker 当作外部请求的来源去收集；本 skill 承接的是开发者自己的改进点。
若某个点已有上游 issue 覆盖，按下方的匹配步骤处理。

## 筛选与分类

- **去重**：把重复或重叠的点合并为一条。
- **剔除**无效、离题或已解决的点。
- **合并**相关点为一条内聚的建议。
- **按领域分类**，匹配 issue 模板选项：固件或示例、BSP 或硬件接口、文档或工具、硬件版本、不确定。

## 检查是否已有对应 issue

对每条保留下来的点，在上游项目的 issue 和 PR 中搜索匹配项：

- 若已存在匹配 issue，**不要重复建**。追加一条评论或标注关联关系即可。
- 若不存在匹配 issue，用上游 `.github/ISSUE_TEMPLATE/feature_request.yml` 的字段起草功能
  建议 issue（问题或使用场景、期望行为、涉及范围、替代方案、补充信息、安全确认）。

## 起草、审查、提交

1. 起草 issue 内容（标题与正文用英文，字段标签按模板定义），去除凭证和私有数据。
2. 把草案交给开发者，等待明确授权。
3. 仅在批准后，通过第一个可用的 GitHub 通道（GitHub MCP、GitHub skill、或
   `gh issue create --repo FoloToy/ai-passport`）提交，并回读确认创建结果。

## 本 skill 不做的事

- 不发布固件、不运行 publisher 流程。
- 不提交代码 PR、不改仓库代码。
- 未经开发者审查与同意，不自动提交任何内容。

## 相关文档

- 项目开发完成流程总览：`docs/development/project-completion.md`
- 提交 issue：`docs/development/file-issues.md`
- 固件发布：`docs/development/publish-to-community.md`
- issue 模板：`.github/ISSUE_TEMPLATE/feature_request.yml`
- 贡献与提交规则：`docs/contribution/commit-and-pr.md`
