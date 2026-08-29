<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 工程规范（Development）

本目录存放 AI Passport 的工程开发规范和可复用工作流。

## 收录标准

- 收录构建验证、代码风格、注释、测试、资源约束和 AI 开发流程。
- `agent-guide.md` 面向 AI 编程助手，允许包含本项目结构和硬件边界。
- 每条规则应写清触发条件、必须做什么、禁止做什么、验证方法和例外条件。
- 涉及本板具体硬件事实的结论引用 `docs/hardware-design/`，不重复。
- 可以由 lint、测试或脚本强制的要求，应同时落实到自动化检查，不能只靠 agent 阅读文字。
- 新增规约时在本文件更新索引。

## 文档索引

- [agent-guide.md](agent-guide.md)：AI 开发工作流（面向 AI 编程助手：上下文建立、需求拆解、BSP 边界、验收交付格式）。
- [environment-setup.zh_CN.md](environment-setup.zh_CN.md)：AI 在全新机器上的环境引导，包含国际与中国大陆下载线路。
- [build-and-test.md](build-and-test.md)：构建与验证（ESP-IDF 命令、逻辑测试、改动验证要求）。
- [ble-recovery-compatibility.zh_CN.md](ble-recovery-compatibility.zh_CN.md)：小程序 BLE
  安装所需的产物、分区与 bootloader 强制契约。
- [coding-conventions.md](coding-conventions.md)：代码约定（语言风格、复用、注释、测试同步、资源约束等）。
- [CI-build-and-release.md](CI-build-and-release.md)：自动构建与发布说明（tag 触发自动编译固件并发布 Release）。
- [CI-validation.md](CI-validation.md)：Pull Request 与 main 的自动仓库检查、host tests 和固件验证。
- [CI-sync-main.md](CI-sync-main.md)：上游同步说明（定期把上游 `FoloToy/ai-passport` 的 `main` 同步到本 fork 的 `main`）。
- [publish-to-community.md](publish-to-community.md)：发布到社区说明（把当前固件发布到 AI Passport 社区市场）。
- [project-completion.md](project-completion.md)：项目开发完成流程说明（一组可选收尾动作）。
- [file-issues.md](file-issues.md)：提交 issue 说明（把建议作为上游 GitHub issue 提交）。
- [experience-notes.md](experience-notes.md)：开发经验索引（`docs/experiences/` 下经验条目的索引）。
