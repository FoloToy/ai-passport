<p align="right">
  <strong>简体中文</strong> · <a href="repository-optimization-plan.md">English</a>
</p>

# 最小仓库实施记录

## 已确认目标

本仓库是支持以下 AI 流程的最小自包含基线：

```text
读取 AGENTS.md -> 搭建 ESP-IDF 5.5.3 -> 实现需求
-> 运行仓库与主机检查 -> 编译 ESP32-C3 固件
-> 验证并输出 build/FoloToy-AI-Passport-full.bin
```

该流程只使用当前 checkout。远程 demo 分支、社区归档、网站、私有文件和开发者专用 shell 配置都不是必需输入。

## 保留的基线

- `AGENTS.md` 是唯一必读入口；`CLAUDE.md` 只把兼容工具重定向到该入口。
- `components/bsp/` 管理量产板实现与公开 API。
- `main/` 准确保留七个可运行页面：显示、按键、音频、电池、Wi-Fi 扫描、Bluetooth LE 广播和 light/deep sleep。
- `tests/` 保存脱离硬件的逻辑测试；`tools/run-host-tests.sh` 是测试执行入口。
- `tools/validate.sh` 是本地与 CI 的唯一验证入口。
- ESP-IDF 输入为 `CMakeLists.txt`、`sdkconfig.defaults`、`partitions.csv`、`dependencies.lock` 和 `.clangd`。
- 维护中文档只包含产品能力、AI 流程、环境搭建、构建/测试/输出、代码与贡献规则、产品规格、板级事实、验收和已发布历史。

## 已删除内容

仓库所有者已授权直接删除且不迁移；Git 历史是恢复来源。

| 已删除区域 | 原因 |
| --- | --- |
| `plays/` | 应用归档不属于固件开发，也不是可执行基线代码 |
| `docs/experiences/` | 叙述性经验与权威工程文档中的事实重复 |
| `skills/` | Issue、经验和归档自动化发生在固件开发之后 |
| `docs/assets/brand/` 与 `docs/brand-and-product.*` | 营销素材和实时网站/目录信息不是固件输入 |
| 社区发布、发布后和 Issue 提交指南 | 这些文档在固件输出后定义第二套生命周期 |
| Fork 指南与自动 fork 同步 workflow | 下游仓库群同步不属于当前 checkout 的构建流程 |
| 空的根素材和软件设计骨架 | 空跟踪结构没有可运行或文档职责 |
| 独立 CI 指南 | 唯一有效内容已经合并到 `build-and-test.md` |

## 2026-08-28 确认的硬件决策

- 引脚与器件映射适用于所有出货的标准量产设备。
- 电池容量为 520 mAh，每台量产设备均安装 CW2017。
- I2C SDA GPIO10 与 SCL GPIO7 分别通过 2.2 kΩ 外部电阻上拉到 3.3 V。
- 量产实测按键窗口保持 `[0,150)`、`[150,447)` 和 `[447,1900)` mV。
- GPIO0 上任意功能键都是 light sleep 与 deep sleep 的低电平唤醒源。
- 外部电源电路管理电源键：持续按住 0.5 秒开机，持续按住 2 秒关机；固件无法读取。
- 功放无 MCU 使能脚，由电池供电，并使用 4.2 V 满电值作为固定 `esp_codec_dev` 增益校准参数。
- 设备二维码/恢复行为不是工程硬件接口，已从仓库规格删除。

## CI 契约

仓库准确保留三个 workflow：

1. `static-checks.yml` 在每个 pull request 和每次 `main` push 运行仓库检查、主机测试和 actionlint。
2. `firmware-checks.yml` 只在固件、配置、组件、分区或固件验证路径改变时使用 ESP-IDF 5.5.3 构建，同时支持手动触发；经过验证的合并镜像保留七天。
3. `build-firmware.yml` 在手动触发和版本 tag 上构建同一镜像；只有 tag 运行创建 GitHub Release。

所有 Action 都锁定完整 commit SHA。所有构建路径都调用 `tools/validate.sh`；YAML 不复制固件合并或验证逻辑。

## 远程工作项处置

整合 PR 取代 PR #21、#24、#26 和 #27。PR #24 中长期有效的 GPIO 唤醒、直接显示和 LVGL 对象安全事实已经写入权威代码与文档；经验归档本身删除。整合 PR 包含 `Closes #22` 和 `Closes #23`，因此两个 Issue 只在实现进入 `main` 后关闭。

## 验证状态

只有同时满足以下条件才接受本次实现：

- `./tools/validate.sh --static` 通过仓库检查、全部主机测试和 workflow lint。
- `./tools/validate.sh --firmware` 使用 ESP-IDF 5.5.3 构建，并按分区偏移逐字节验证合并镜像。
- `./tools/validate.sh` 通过完整门禁。
- `git diff --check` 不报告空白错误。
- 全新 checkout 不依赖远程 demo 分支即可运行文档命令。
- 实机验收覆盖每个页面，并分别测试 UP、DOWN、OK 对 light sleep 与 deep sleep 的唤醒。

在用户连接标准量产板之前，设备测试状态为 `NOT RUN`。在两台未安装 ESP-IDF 的机器分别执行国际与中国大陆线路之前，两条全新机器环境引导状态为 `NOT RUN`。这些状态是明确测试结果，不是实现假设。
