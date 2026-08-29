<p align="right">
  <strong>简体中文</strong> · <a href="INDEX.md">English</a>
</p>

# Docs 规范索引

本索引用于发现 `docs/` 下的全部文档：协作规范、工程规范、fork 工作流、软硬件设计文档。

**状态含义**：`authoritative` = 权威、对开发与协作有约束力；`参考` = 设计 / 记录 / 骨架，供背景参考。

| 文档 | 类型 | 状态 | 说明 |
| --- | --- | --- | --- |
| [CHANGELOG.zh_CN.md](./CHANGELOG.zh_CN.md) | 变更记录 | authoritative | 用户可见行为、兼容性与发布流程历史 |
| [brand-and-product.zh_CN.md](./brand-and-product.zh_CN.md) | 品牌与产品说明 | authoritative | 品牌与产品定位、官方入口、开源与授权、产品规格引用 |
| [contribution/README.zh_CN.md](./contribution/README.zh_CN.md) | 协作规范索引 | authoritative | 通用协作规范（文档规范、提交与 PR 约定） |
| [contribution/doc-conventions.zh_CN.md](./contribution/doc-conventions.zh_CN.md) | 文档规范 | authoritative | 按任务加载上下文、文档职责、写作维护和内容安全 |
| [contribution/commit-and-pr.zh_CN.md](./contribution/commit-and-pr.zh_CN.md) | 协作规范 | authoritative | 提交规范 + 提交与 PR 约定 |
| [development/README.zh_CN.md](./development/README.zh_CN.md) | 工程规范索引 | authoritative | 通用工程开发规范（构建验证、代码约定） |
| [development/environment-setup.zh_CN.md](./development/environment-setup.zh_CN.md) | 工程规范 | authoritative | AI 全新机器环境引导、国际与中国大陆下载线路 |
| [development/build-and-test.zh_CN.md](./development/build-and-test.zh_CN.md) | 工程规范 | authoritative | 构建与验证（ESP-IDF 命令、逻辑测试、改动验证要求） |
| [development/ble-recovery-compatibility.zh_CN.md](./development/ble-recovery-compatibility.zh_CN.md) | 工程规范 | authoritative | 小程序 BLE 固件产物、分区与 bootloader 兼容契约 |
| [development/coding-conventions.zh_CN.md](./development/coding-conventions.zh_CN.md) | 工程规范 | authoritative | 代码约定（语言风格、复用、注释、测试同步、资源约束） |
| [development/agent-guide.zh_CN.md](./development/agent-guide.zh_CN.md) | 工程规范 | authoritative | AI 开发工作流（上下文建立、需求拆解、BSP 边界、验收交付格式） |
| [development/CI-build-and-release.zh_CN.md](./development/CI-build-and-release.zh_CN.md) | CI 文档 | authoritative | 自动构建与发布说明（tag 触发自动编译固件并发布 Release） |
| [development/CI-validation.zh_CN.md](./development/CI-validation.zh_CN.md) | CI 文档 | authoritative | PR/main 自动仓库检查、host tests 与固件验证 |
| [development/CI-sync-main.zh_CN.md](./development/CI-sync-main.zh_CN.md) | CI 文档 | authoritative | 上游同步说明（定期同步上游 main 到 fork） |
| [development/publish-to-community.zh_CN.md](./development/publish-to-community.zh_CN.md) | 工程规范 | authoritative | 发布到 AI Passport 社区市场 |
| [development/project-completion.zh_CN.md](./development/project-completion.zh_CN.md) | 工程规范 | authoritative | 项目开发完成流程（可选收尾动作） |
| [development/file-issues.zh_CN.md](./development/file-issues.zh_CN.md) | 工程规范 | authoritative | 把建议提交成上游 GitHub issue |
| [development/experience-notes.zh_CN.md](./development/experience-notes.zh_CN.md) | 沉淀记录 | 参考 | `docs/experiences/` 下开发经验条目的索引 |
| [fork-guide.zh_CN.md](./fork-guide.zh_CN.md) | fork 工作流 | authoritative | 目录结构、main 保持干净、fork 约定、docs/assets 使用 |
| [software-design/README.zh_CN.md](./software-design/README.zh_CN.md) | 软件设计索引 | 参考 | 软件设计文档子目录骨架 |
| [hardware-design/README.zh_CN.md](./hardware-design/README.zh_CN.md) | 硬件设计索引 | 参考 | 硬件设计文档子目录骨架 |
| [hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md](./hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md) | 硬件指南 | authoritative | 完整硬件开发指南与排障参考（上游） |
| [hardware-design/specifications.zh_CN.md](./hardware-design/specifications.zh_CN.md) | 产品规格 | authoritative | 产品规格（尺寸、重量、电池、充电、NFC、按键等对外口径） |
| [../plays/INDEX.zh_CN.md](../plays/INDEX.zh_CN.md) | plays 应用档案索引 | 参考 | `plays/` 下已归档应用的发现表 |

## GitHub 社区治理文档

以下社区治理文档位于 `.github/`（GitHub 自动识别）：

- [CONTRIBUTING.zh_CN.md](../.github/CONTRIBUTING.zh_CN.md)：贡献指南（开发验证、PR 流程、许可）。
- [CODE_OF_CONDUCT.zh_CN.md](../.github/CODE_OF_CONDUCT.zh_CN.md)：贡献者公约行为准则。
- [SECURITY.zh_CN.md](../.github/SECURITY.zh_CN.md)：安全漏洞报告流程。
- [SUPPORT.zh_CN.md](../.github/SUPPORT.zh_CN.md)：使用支持与问题反馈渠道。
