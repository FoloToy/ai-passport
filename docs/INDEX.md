# Docs 规范索引

本索引用于发现 `docs/` 下的全部文档：协作规范、工程规范、fork 工作流、软硬件设计文档。

**状态含义**：`authoritative` = 权威、对开发与协作有约束力；`参考` = 设计 / 记录 / 骨架，供背景参考。

| 文档 | 类型 | 状态 | 说明 |
| --- | --- | --- | --- |
| [brand-and-product.md](./brand-and-product.md) | 品牌与产品说明 | authoritative | 品牌与产品定位、官方入口、开源与授权、产品规格引用 |
| [contribution/README.md](./contribution/README.md) | 协作规范索引 | authoritative | 通用协作规范（文档规范、提交与 PR 约定） |
| [contribution/doc-conventions.md](./contribution/doc-conventions.md) | 文档规范 | authoritative | 先读所有 README、文档生成规范、文档分类规范（公共/特定项目） |
| [contribution/commit-and-pr.md](./contribution/commit-and-pr.md) | 协作规范 | authoritative | 提交规范 + 提交与 PR 约定 |
| [development/README.md](./development/README.md) | 工程规范索引 | authoritative | 通用工程开发规范（构建验证、代码约定） |
| [development/build-and-test.md](./development/build-and-test.md) | 工程规范 | authoritative | 构建与验证（ESP-IDF 命令、逻辑测试、改动验证要求） |
| [development/coding-conventions.md](./development/coding-conventions.md) | 工程规范 | authoritative | 代码约定（语言风格、复用、注释、测试同步、资源约束） |
| [development/agent-guide.md](./development/agent-guide.md) | 工程规范 | authoritative | AI 开发工作流（上下文建立、需求拆解、BSP 边界、验收交付格式） |
| [development/CI-build-and-release.md](./development/CI-build-and-release.md) | CI 文档 | authoritative | 自动构建与发布说明（tag 触发自动编译固件并发布 Release） |
| [development/CI-sync-main.md](./development/CI-sync-main.md) | CI 文档 | authoritative | 上游同步说明（定期同步上游 main 到 fork） |
| [fork-guide.md](./fork-guide.md) | fork 工作流 | authoritative | 目录结构、main 保持干净、fork 约定、docs/assets 使用 |
| [software-design/README.md](./software-design/README.md) | 软件设计索引 | 参考 | 软件设计文档子目录骨架 |
| [hardware-design/README.md](./hardware-design/README.md) | 硬件设计索引 | 参考 | 硬件设计文档子目录骨架 |
| [hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md](./hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md) | 硬件指南 | authoritative | 完整硬件开发指南与排障参考（上游） |
| [hardware-design/specifications.md](./hardware-design/specifications.md) | 产品规格 | authoritative | 产品规格（尺寸、重量、电池、充电、NFC、按键等对外口径） |

## 仓库根治理文档

以下社区治理文档位于仓库根目录（GitHub 自动识别）：

- [CONTRIBUTING.md](../CONTRIBUTING.md)：贡献指南（开发验证、PR 流程、许可）。
- [CODE_OF_CONDUCT.md](../CODE_OF_CONDUCT.md)：贡献者公约行为准则。
- [SECURITY.md](../SECURITY.md)：安全漏洞报告流程。
- [SUPPORT.md](../SUPPORT.md)：使用支持与问题反馈渠道。
