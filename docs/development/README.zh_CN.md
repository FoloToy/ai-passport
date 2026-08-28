<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 工程规范

本目录包含从应用需求到经过验证的合并固件镜像的完整工程路径。硬件事实保留在 `docs/hardware-design/`。

## 文档

- [agent-guide.zh_CN.md](agent-guide.zh_CN.md)：代码修改流程、事实优先级、BSP 边界、运行时规则与交付字段。
- [environment-setup.zh_CN.md](environment-setup.zh_CN.md)：国际与中国大陆网络下的全新机器 ESP-IDF 5.5.3 环境搭建。
- [build-and-test.zh_CN.md](build-and-test.zh_CN.md)：准确本地检查、固件输出、CI 触发条件、发布行为与实机测试边界。
- [coding-conventions.zh_CN.md](coding-conventions.zh_CN.md)：C、LVGL、命名、资源与主机测试规则。
- [repository-optimization-plan.zh_CN.md](repository-optimization-plan.zh_CN.md)：最小 AI 到固件基线的决策与实施记录。

每条新增工程规则都必须写明触发条件、必须动作、禁止动作和验证命令。可自动检查的规则还必须落实到 `tools/` 或 CI。
