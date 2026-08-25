<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 软件设计（Software Design）

本目录用于存放本仓库的软件设计文档，包括应用层、组件层与系统层的设计说明、模块划分、接口约定、状态机、资源约束与测试策略。

## 目录约定

- 每个设计主题一个子目录或一个独立 `.md` 文件，名称与主题一致，便于检索。
- 文档面向 AI agent 与开发者，应包含：目标、范围、输入/输出、状态、并发任务、持久化、内存预算与失败降级。
- 涉及硬件事实的结论在 `hardware-design/` 中维护；软件设计只依赖其稳定的接口。

## 如何添加一篇软件设计文档

1. 在本目录下新建一个描述性文件（如 `xxx-design.md`）或子目录。
2. 在文档顶部写明适用范围与所属版本/提交。
3. 若与硬件行为强相关，链接到 `docs/hardware-design/` 对应文档，不要在此重复硬件数据。

## 现有文档索引

- [AGENTS.md](../../AGENTS.md)：仓库权威 AI 规范的入口与索引。

> 注：`docs/software-design` 用于容纳软件设计文档。协作规范见 `docs/contribution/`，工程规范和 CI 说明见 `docs/development/`，fork 工作流见 `docs/fork-guide.md`；这些入口均由 `AGENTS.md` 索引。
