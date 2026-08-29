<p align="right">
  <strong>简体中文</strong> · <a href="file-issue.md">English</a>
</p>

# 动作 F：提交 issue

本动作收集发布固件的开发者本人的改进点，把有价值的整理成功能建议 issue，提交到上游项目。它是[项目开发完成流程](../project-completion.md)列出的六项可选动作之一。

工作流由 `issue-suggestions` skill 驱动。issue 提交到上游项目，而不是 fork。

## 步骤

1. 确认同意与可用的 GitHub 通道（GitHub MCP、GitHub skill 或 `gh`）。
2. 收集开发者在开发或发布该版本过程中遇到的自身改进点。
3. 去重、剔除无效或已解决的点，并按影响区域分类。
4. 与已有 issue 和 PR 匹配；不建重复项。
5. 使用上游 issue 模板起草功能建议。
6. 提交前把草案展示给开发者并取得明确批准。
7. 通过第一个可用的 GitHub 通道提交，并读回创建的 issue 确认。

## 安全

- 绝不包含凭证、设备 QR 密钥、私密设备链接、个人数据或未脱敏日志。
- 安全漏洞走 `.github/SECURITY.md`，不通过公开 issue。

## 相关文档

- 提交 issue 参考：[file-issues.md](../file-issues.md)
- Skill：[issue-suggestions](../../../skills/issue-suggestions/SKILL.md)
- issue 模板：[.github/ISSUE_TEMPLATE/feature_request.yml](../../../.github/ISSUE_TEMPLATE/feature_request.yml)
