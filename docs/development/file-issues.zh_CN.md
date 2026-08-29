<p align="right">
  <strong>简体中文</strong> · <a href="file-issues.md">English</a>
</p>

# 提交 issue

本页说明如何把建议作为 GitHub issue 提交到上游 `FoloToy/ai-passport` 项目。它是
`issue-suggestions` skill 的文档配套；skill 负责驱动流程，本页说明规则与约定。

issue 用于提交围绕固件、BSP、硬件或文档的用户建议、功能请求与使用反馈。它们提交到上游项目，
而不是 fork。

## issue 类型与模板

仓库在 `.github/ISSUE_TEMPLATE/` 下定义了 issue 模板。选择匹配的报告类型：

- `feature_request.yml`：新功能或改进。
- `usage_question.yml`：构建、烧录、配置或使用设备方面的求助。

完整填写模板字段。功能建议使用这些字段：

- 问题或使用场景
- 期望行为与验收标准
- 涉及范围（固件或示例、BSP 或硬件接口、文档或工具、硬件版本、不确定）
- 已考虑的替代方案
- 补充信息
- 安全确认

面向上游的 issue，标题与正文用英文书写；模板的字段标签按模板定义保留双语。

## 收集与筛选

收集发布固件的开发者在开发或发布本次版本时遇到的改进点，然后筛选：

- 把重复或重叠的建议去重合并为一条。
- 剔除无效、离题或已解决的条目。
- 把相关条目合并成内聚的功能建议。
- 与已有 issue 和 PR 匹配；不要重复建。

## 提交前审查与同意

提交 issue 是公开、对外可见的动作，且涉及项目相关内容。遵守以下门槛：

1. 与开发者确认他们同意开始本次收尾。
2. 确认已有可用的 GitHub 通道，依次尝试 GitHub MCP、GitHub skill、再退到 `gh` CLI；若都不可用，
   则把草案交给开发者手动粘贴。
3. 起草 issue，交给开发者，等待明确批准。
4. 批准后通过第一个可用的 GitHub 通道（GitHub MCP、GitHub skill、或
   `gh issue create --repo FoloToy/ai-passport`）创建，并回读确认。

## 安全

永远不要包含凭证、设备 QR 密钥、私密设备链接、个人数据或未脱敏日志。安全漏洞走
`.github/SECURITY.md`，不要用公开 issue。

## 相关文档

- 项目开发完成流程总览：[project-completion.md](project-completion.md)
- issue skill：[`../../skills/issue-suggestions/SKILL.md`](../../skills/issue-suggestions/SKILL.md)
- 固件发布：[publish-to-community.md](publish-to-community.md)
