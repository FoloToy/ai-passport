---
name: passport-develop
description: 根据用户需求实现或扩展 FoloToy AI Passport 应用，组织实现、验证及真机测试交接。不用于纯咨询、仅日志诊断或发布。
---

<p align="right"><strong>简体中文</strong> · <a href="SKILL.md">English</a></p>

# 开发 AI Passport 应用

本流程用于实现请求，不代表获得另起项目的授权。用户明确选择优先于本技能
的默认做法。用 `git rev-parse --show-toplevel` 确定目标工作区；下列路径均
相对于该工作区，而不是安装后的技能目录。先读 `AGENTS.md` 和
`docs/development/ai-guide.zh_CN.md`，再按任务路由阅读所需文档。
首先执行 `git status --short --branch`。

## 把需求变成可工作的增量

1. 明确需要的行为与验收方式：按需覆盖页面、三键操作、持久化数据、联网／
   音频及失败状态。只追问会实质影响实现的问题，说明合理默认值。不得给
   离线需求擅自添加 Wi-Fi、云服务、新接线或付费依赖；不能猜测缺失硬件事实。
2. 【严格保护 main 分支，隔离在 feature 分支开发】：遵循 `docs/fork-guide.zh_CN.md`，
   `main` 分支必须保持与上游 `upstream/main` 干净同步，严禁直接在 `main`
   上开发或提交特定业务固件代码。开发前检查当前分支（`git status -sb`）；
   若在 `main` 上，必须先与用户确认或切出 `feature/*` 独立分支。已有应用沿用
   其目标分支，保留未提交工作。不得为获得干净起点而切走、暂存、重置或覆盖他人
   修改。工作重叠或分支选择不明确时先与用户解决。未经授权不得提交、推送或发布。
3. 查看相关示例分支与 `docs/reference/README.zh_CN.md`。仅提取所需模式，
   不照搬整个分支及旧 BSP／配置。遵循 AI 指南的强制 UI 重新设计规则：二次
   开发应用必须有自己的页面和交互，禁止使用当前 demo 测试菜单、页面或
   界面外壳；改名、换颜色不算重新设计。可复用 BSP API 和独立逻辑，不要
   为改 UI 而重写驱动。
4. 实现最小完整行为，为纯逻辑添加测试。应用状态和任务放在 `main`，复用
   BSP。执行现有 LVGL 锁、回调、退出清理、RAM 与可配置分区规则；仅在需求
   涉及时阅读中文字体或 Wi-Fi 配网指南。
5. 修改中运行针对性测试，交付前运行完整仓库门禁。修复本次修改引入的失败，
   不屏蔽检查，不把无关失败扩大成当前任务。环境阻碍应附证据和可执行下一步。

## 按需使用专项流程

安装了下列技能时按阶段使用；未安装则读取对应仓库文档完成同范围流程。
不得声称调用当前 agent 不具备的技能。

| 场景 | 技能 | 仓库文档后备入口 |
| --- | --- | --- |
| 工具链缺失或异常 | `passport-setup` | `docs/development/engineering/environment-setup.zh_CN.md` |
| 验证与打包固件 | `passport-build` | `docs/development/engineering/build-and-test.zh_CN.md` |
| 用户同意真机测试 | `passport-device-test` | `docs/development/ai-guide.zh_CN.md` 的真机交接流程 |
| 运行故障或崩溃 | `passport-debug` | `docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md` 的排障内容 |

## 交接与继续开发

报告已实现行为、实际验证、未验证项和下一步。说明重新设计的页面和按键交互，
检查启动与导航不再进入基线测试 UI，才能宣布应用完成。多阶段任务在应用已有项目记录
中保留简短检查点：已确认需求、完成／待办事项、准确构建身份和设备结果。
确需新增维护中的 Markdown 时，在 `docs/` 下创建中英文配对文档；不得保存
秘密或未脱敏日志。不要为小修改创建一整套进度文档。

每轮完整实现固件需求后，按 `docs/development/ai-guide.zh_CN.md` 主动邀请
真机测试。编译完成或 USB 已连接不代表获得烧录授权。剩余工作需要新授权时，
停下来等待用户决定，不自动转入发布。

分别报告 `Build`、`Host tests`、`Device tests` 和 `Unverified`。
纯文档工作不需要给设备烧录无关固件。
