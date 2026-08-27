<p align="right">
  <strong>简体中文</strong> · <a href="commit-and-pr.md">English</a>
</p>

# 提交与 PR 规范（Commit & PR）

## 提交规范

- commit 标题使用英文祈使句和 Conventional Commit 格式：`type(scope): description`。
- `type` 使用 `feat`、`fix`、`docs`、`refactor`、`perf`、`test`、`chore`、`build` 或 `ci`；标题不超过 72 个字符，结尾不加句号。
- 一个 commit 只表达一个可独立 review 的逻辑变化，message 描述最终 diff，不记录调试过程。
- 提交前检查完整 diff，运行适用测试，并排除凭证、生成固件和无关文件。
- 仅用户可见的行为、兼容性和发布流程变化需要更新 `docs/CHANGELOG.zh_CN.md`；内部重构、CI 维护、拼写修复和依赖锁刷新不强制记录。
- 不因创建 commit 自动获得 push、创建 PR、发布 Release 或合并的权限；这些外部操作由用户请求或明确工作流授权。
- 需求形成了需要长期维护的产品约束、数据格式、架构决定或验收标准时，写回对应设计/规范文档；临时讨论和调试记录不必落仓。

## PR 约定

- PR 标题使用英文，采用 Conventional Commit 格式和英文祈使句，例如 `docs: streamline AI development workflow`。
- PR body 使用英文，至少填写 `.github/PULL_REQUEST_TEMPLATE.md` 中的摘要、兼容性和验证结果。
- Build、Host tests 和 Device tests 必须分开报告；未执行的真机项目写入 `Unverified`，不能用编译成功代替实机验证。
- 对引脚、显示旋转、codec 时钟、ADC、DMA、Flash 布局或电源行为的修改，必须记录板卡版本和观察到的真机结果；未验证时不得标记 ready for merge。
- 显示类改动附照片或截图；涉及接线、引脚映射、持久化格式或兼容性的改动必须显式说明。
