<p align="right">
  <strong>简体中文</strong> · <a href="CONTRIBUTING.md">English</a>
</p>

# 贡献指南

感谢你为自包含的 FoloToy AI Passport 固件基线贡献代码、文档、固件或反馈。

## 开始之前

- 先读 [`AGENTS.zh_CN.md`](../AGENTS.zh_CN.md)：它是 AI agent 应遵循规则的权威入口与索引，不是本指南的替代品。
- 先读 [`README.zh_CN.md`](../docs/README.zh_CN.md) 的硬件能力契约，以及
  [AI 硬件开发指南](../docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md) 的完整硬件上下文。
- 参与社区时请遵守 [`CODE_OF_CONDUCT.md`](CODE_OF_CONDUCT.md)；普通使用问题见 [`SUPPORT.md`](SUPPORT.md)。
- 不要提交凭证、令牌、授权文件或个人数据。
- 贡献分支必须从 `main` 创建；应用不得依赖远程 demo 分支。

## 开发与验证

使用 ESP-IDF 5.5.3。全新机器先按
[环境引导](../docs/development/environment-setup.zh_CN.md)完成安装。

交付必须运行仓库门禁。直接 IDF 命令只用于增量开发。

```bash
source <ESP-IDF-v5.5.3-路径>/export.sh
idf.py --version             # 必须输出 ESP-IDF v5.5.3
./tools/validate.sh --firmware # 必须生成合并固件
idf.py set-target esp32c3     # 配置目标芯片（fresh checkout 后/换 target 后运行）
idf.py build                  # 仅用于增量 app 编译
idf.py flash monitor          # 仅用于增量 app 烧录
idf.py fullclean              # 配置过期时清空生成状态（勿用于清理用户源码改动）
```

所有脱离硬件的测试通过仓库 runner 执行：

```bash
./tools/run-host-tests.sh
```

本仓库为本地开发和 CI 提供同一个验证入口：

```bash
./tools/validate.sh --static
./tools/validate.sh --firmware  # 需要先激活 ESP-IDF 5.5.3 环境
./tools/validate.sh             # 完整门禁
```

遵守 [`docs/development/build-and-test.zh_CN.md`](../docs/development/build-and-test.zh_CN.md) 的
验证要求：干净的 `idf.py build` 是最低自动检查，不是硬件验收；构建结果与真机结果
分开记录，禁止把编译通过描述成硬件验证通过。

## 提交 Pull Request

1. 从 `main` 创建短生命周期 `feature/*` 分支，保持一个 PR 只解决一个清晰的问题。
2. PR 标题使用 `<type>(<scope>): <简短描述>`，例如 `feat(bsp): ...`、`docs: ...`。
   可用 type 见 [`docs/contribution/commit-and-pr.zh_CN.md`](../docs/contribution/commit-and-pr.zh_CN.md)。
3. Review 完整 diff，确认没有凭证、无关生成文件或意外改动。
4. 遵守 `docs/contribution/commit-and-pr.md` 的 PR 要求：说明测试的硬件/版本、
   行为变更摘要、构建与真机结果，链接相关 issue，并对引脚、显示旋转、codec 时钟、
   ADC、DMA 改动显式记录观察到的真机结果。
5. 等待 CI 和 review；除维护者显式例外外，不要直接向 `main` 推送。

修改电气映射、公开 BSP API、分区布局或设备持久化格式前必须先开 Issue。其它范围单一的变更直接建立 PR。

## 贡献的许可

本仓库使用 [MIT](../LICENSE) 许可证。提交贡献即表示你同意按本仓库的 MIT 条款并入你的贡献。

## 安全漏洞

不要在公开 issue、Pull Request 或讨论中披露漏洞、凭证或可利用细节。请按
[`SECURITY.md`](SECURITY.md) 的私密报告流程处理。
