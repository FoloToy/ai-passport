# 工程规范（Development）

本目录存放通用工程开发规范（公共文档，可提上游）。

## 收录标准

- 只收录通用工程开发规范（构建验证、代码风格、注释、测试、资源约束等），对任何项目成立。
- `agent-guide.md` 为面向 AI 编程助手的开发工作流说明，含本项目结构细节，与通用规范并列维护。
- 每条规则应写清触发条件、必须做什么、禁止做什么、验证方法和例外条件。
- 涉及本板具体硬件事实的结论引用 `docs/hardware-design/`，不重复。
- 可以由 lint、测试或脚本强制的要求，应同时落实到自动化检查，不能只靠 agent 阅读文字。
- 新增规约时在本文件更新索引。

## 文档索引

- [agent-guide.md](agent-guide.md)：AI 开发工作流（面向 AI 编程助手：上下文建立、需求拆解、BSP 边界、验收交付格式）。
- [build-and-test.md](build-and-test.md)：构建与验证（ESP-IDF 命令、逻辑测试、改动验证要求）。
- [coding-conventions.md](coding-conventions.md)：代码约定（语言风格、复用、注释、测试同步、资源约束等）。
- [CI-build-and-release.md](CI-build-and-release.md)：自动构建与发布说明（tag 触发自动编译固件并发布 Release）。
- [CI-sync-main.md](CI-sync-main.md)：上游同步说明（定期把上游 `FoloToy/ai-passport` 的 `main` 同步到本 fork 的 `main`）。
