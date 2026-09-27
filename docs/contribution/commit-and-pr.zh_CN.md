<p align="right">
  <strong>简体中文</strong> · <a href="commit-and-pr.md">English</a>
</p>

# 提交与 PR 规范（Commit & PR）

## 提交规范

- commit 标题使用英文祈使句和 Conventional Commit 格式：`type(scope): description`。
- `type` 使用 `feat`、`fix`、`docs`、`refactor`、`perf`、`test`、`chore`、`build` 或 `ci`；标题不超过 72 个字符，结尾不加句号。
- 细粒度提交：一个 commit 只承载一个可独立 review、可独立回滚的逻辑变化。按单元拆分，
  例如驱动修复、对应 host test、文档同步应为三个 commit，而不是合成一个。
- 长任务按单元推进：每完成一个逻辑单元就提交，不要等整个任务做完再合成一个大 commit。
- commit 正文用英文写详细，不能只写标题：说明最终 diff 改了什么、为什么需要这个改动、
  实际验证了什么。正文描述最终 diff，不记录调试过程。
- 每次提交前运行适用的检查；单元之间不要求跑完整的 ESP-IDF 构建，任务收尾时跑一次完整门禁。
- 检查完整 diff，排除凭证、生成固件和无关文件。
- 普通功能、应用和文档 commit 不修改中英文变更日志。用户可见行为、兼容性和发布流程影响写入 PR 正文；只有打 tag 前的发布准备变更，才把已合并内容统一汇总到两份变更日志。
- 不因创建 commit 自动获得 push、创建 PR、发布 Release 或合并的权限；这些外部操作由用户请求或明确工作流授权。
- 需求形成了需要长期维护的产品约束、数据格式、架构决定或验收标准时，写回对应设计/规范文档；临时讨论和调试记录不必落仓。

## PR 约定

- PR 标题使用英文，采用 Conventional Commit 格式和英文祈使句，例如 `docs: streamline AI development workflow`。
- PR body 使用英文，至少填写 `.github/PULL_REQUEST_TEMPLATE.md` 中的摘要、兼容性和验证结果。
- Build、Host tests 和 Device tests 必须分开报告；未执行的真机项目写入 `Unverified`，不能用编译成功代替实机验证。
- 对引脚、显示旋转、codec 时钟、ADC、DMA、Flash 布局或电源行为的修改，必须记录板卡版本和观察到的真机结果；未验证时不得标记 ready for merge。
- 显示类改动附照片或截图；涉及接线、引脚映射、持久化格式或兼容性的改动必须显式说明。
- 开往上游 `FoloToy/ai-passport` 项目的 PR 中，不得新增或修改根目录 `README.md` / `README.zh_CN.md` —— 根 README 是 fork owner 自留内容（上游概览在 `docs/README.md`）。
