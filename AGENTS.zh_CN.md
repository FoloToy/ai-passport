<p align="right">
  <strong>简体中文</strong> · <a href="AGENTS.md">English</a>
</p>

# AI Agent 仓库规范

本文是本仓库 AI 辅助工作的唯一必读入口。根据下方路由表读取当前任务所需文档，不要默认加载全部 README。

## 项目与安全基线

- 目标平台：ESP32-C3、8 MB Flash、无 PSRAM、ESP-IDF 5.5.3。
- 必须保持小程序 BLE 安装兼容：3 MB 应用上限、`cardid@0x356000`、
  永久 `recovery@0x700000` 以及上键持续 5 秒进入 Recovery 的 bootloader hook
  均为模板强制契约。
- 保留用户已有修改。先执行 `git status --short --branch`，不得覆盖或清理无关文件。
- 硬件事实优先级：产品规格与实测结果 → `components/bsp/include/bsp_pins.h` → BSP 头文件与实现 → 硬件指南 → README/demo。任务所需硬件细节未在这些来源中定义时，直接询问用户，不得猜测。
- 可复用板级逻辑放入 `components/bsp`；页面、状态机、动画和应用任务放入 `main`。
- LVGL 非线程安全。LVGL 任务之外访问 LVGL 对象时必须持有 `bsp_lvgl_lock()`。
- 按键回调不得阻塞。音频、存储、网络等慢操作必须放入工作任务。
- demo 删除 screen 前，必须停止所有可能访问其 UI 的任务、定时器、回调和事件处理器。
- 可测试的状态机、协议、计时和布局计算应与 ESP-IDF/LVGL 解耦，并由 host tests 覆盖。
- 禁止提交凭证、设备二维码秘密、私钥、个人数据或未脱敏日志。
- 所有维护中的 Markdown 默认 `.md` 路径必须为英文，简体中文使用配对的 `.zh_CN.md` 文件。两种语言必须保持一致并保留互相切换链接。

## 按任务加载上下文

| 任务 | 修改前读取 |
| --- | --- |
| 任意代码修改 | `docs/development/agent-guide.zh_CN.md`、相关头文件和相邻实现 |
| 环境引导或缺少工具链 | `docs/development/environment-setup.zh_CN.md` |
| BSP、引脚、总线、显示、音频、电池 | `docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md`、`components/bsp/include/bsp_pins.h` |
| Demo 或菜单 | `main/demo.h`、`main/main.c`、最近的 `main/demo_*.c` 实现 |
| 构建、测试、依赖、分区 | `docs/development/build-and-test.zh_CN.md`、`docs/development/ble-recovery-compatibility.zh_CN.md`、`sdkconfig.defaults`、`partitions.csv` |
| CI 或发布 | `docs/development/CI-*.zh_CN.md` 中的对应文件与 `.github/workflows/` |
| 项目开发完成 | `docs/development/project-completion.zh_CN.md`（再进入 `issue-suggestions` 或 `experience-pr` skill） |
| 文档 | `docs/contribution/doc-conventions.zh_CN.md`、`docs/INDEX.zh_CN.md` |
| Commit 或 PR | `docs/contribution/commit-and-pr.zh_CN.md` |

产品概览见 `docs/README.zh_CN.md`；需要发现更多文档时读 `docs/INDEX.zh_CN.md`。Fork 专用流程见 `docs/fork-guide.zh_CN.md`，普通上游开发无需读取。

## 必须执行的验证与交付格式

迭代时运行最小相关检查，交付前运行完整门禁：

```bash
./tools/validate.sh --static    # 仓库检查 + host tests
./tools/validate.sh --firmware  # ESP-IDF 构建 + 合并镜像验证
./tools/validate.sh             # 完整门禁
```

完整门禁要求已激活 ESP-IDF 5.5.3 环境。不得把编译成功描述成硬件验证成功。最终交付必须分别报告：

```text
Build: PASS / FAIL / NOT RUN
Host tests: PASS / FAIL / NOT RUN
Device tests: PASS / FAIL / NOT RUN
Unverified: 仍需板卡、仪器或用户确认的事项
```

仅在用户请求或当前工作流明确要求时创建 commit 和 push。用户可见变化记录到 `docs/CHANGELOG.zh_CN.md`；内部重构、CI 维护、拼写修复和生成文件刷新无需记录。

社区规范见 `.github/CONTRIBUTING.zh_CN.md`、`.github/CODE_OF_CONDUCT.zh_CN.md`、`.github/SECURITY.zh_CN.md` 与 `.github/SUPPORT.zh_CN.md`。
