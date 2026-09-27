<p align="right">
  <strong>简体中文</strong> · <a href="AGENTS.md">English</a>
</p>

# AI Agent 仓库规范

本文是本仓库 AI 辅助工作的唯一必读入口。根据下方路由表读取当前任务所需文档，不要默认加载全部 README。

## 必需 AI 技能

以下五个技能是本仓库 AI 辅助开发的必需项：`passport-develop`、
`passport-setup`、`passport-build`、`passport-device-test`、`passport-debug`。
技能权威源文件位于 `skills/`。

开始开发前，AI 必须自行检查这五个技能是否已在当前环境安装并可用。缺少时，
由 AI 根据所用工具和环境，自行选择合适的安装方式、完成安装并验证可用性。
不等待用户另外提出安装要求，也不把选择安装方式或执行安装命令交给用户。
不强制使用特定脚本、安装目录或 AI 客户端。

安装必须遵守环境的权限审批要求，保留已有技能和用户配置。权限、冲突或平台
限制导致无法安装时，说明阻碍，只请求必要的用户配合，不得谎报安装成功。
五个技能必须可用，不代表每次任务都加载或调用全部技能；只使用匹配当前任务
的技能。烧录、Git 写操作和发布仍须分别取得相应授权。

## 项目与安全基线

- 目标平台：ESP32-C3、8 MB Flash、无 PSRAM、ESP-IDF 5.5.3。
- 仓库默认分区表保持最简：只包含 NVS、PHY data，以及占用 8 MB Flash
  剩余空间的单个 factory app。用户固件可以按需求明确调整布局；修改后必须
  验证结果，不得把产品专用分区变成模板的强制契约。
- 保留用户已有修改。先执行 `git status --short --branch`，不得覆盖或清理无关文件。
- 下载（烧录）新固件无需备份设备内原有固件，不得把读取 Flash 备份作为前置条件。这不保证保留用户数据，也不授权全片擦除；遵循[烧录与数据说明](docs/development/engineering/firmware-layout.zh_CN.md#烧录与已存数据)。
- 必须先问：有任何不明确之处，先问清楚再写代码，不要先做后确认。需求、页面流程、
  按键映射或数据行为存在多种理解时必须问；改动涉及新增接线、电路安全或板卡改版时
  必须问；涉及不可逆数据格式时必须问；删除或覆盖已有内容前必须问。同类问题合并为
  一次提问，并给出推荐选项，而不是开放式询问；Claude Code 用 `AskUserQuestion`，
  其他客户端用最接近的结构化提问方式。不触发以上情况时可选用保守默认值继续，但
  必须在交付中列出全部假设。
- 硬件事实优先级：产品规格与实测结果 → `components/bsp/include/bsp_pins.h` → BSP 头文件与实现 → 硬件指南 → README/demo。任务所需硬件细节未在这些来源中定义时，直接询问用户，不得猜测。
- 可复用板级逻辑放入 `components/bsp`；页面、状态机、动画和应用任务放入 `main`。
- 二次开发应用必须重新设计并实现独立 UI，禁止沿用当前 demo 测试菜单、页面或界面外壳；仅改名、换颜色或在原界面增加功能不算重新设计。BSP API 和非 UI 逻辑仍可复用。详见[二次开发 UI 强制重新设计规则](docs/development/ai-guide.zh_CN.md#二次开发-ui-强制重新设计)。
- LVGL 非线程安全。LVGL 任务之外访问 LVGL 对象时必须持有 `bsp_lvgl_lock()`。
- 添加中文 UI 前必须遵循[字体检查清单](docs/development/engineering/coding-conventions.zh_CN.md#中文字体与缺字排查)。默认 Montserrat 字体不含中文字形；UTF-8 正确、编译成功均不代表能够显示中文。必须核对字形覆盖、控件实际字体并完成真机显示验收。
- 按键回调不得阻塞。音频、存储、网络等慢操作必须放入工作任务。
- demo 删除 screen 前，必须停止所有可能访问其 UI 的任务、定时器、回调和事件处理器。
- 可测试的状态机、协议、计时和布局计算应与 ESP-IDF/LVGL 解耦，并由 host tests 覆盖。
- 禁止提交凭证、设备二维码秘密、私钥、个人数据或未脱敏日志。
- 所有维护中的 Markdown 默认 `.md` 路径必须为英文，简体中文使用配对的 `.zh_CN.md` 文件。两种语言必须保持一致并保留互相切换链接。

## 按任务加载上下文

| 任务 | 修改前读取 |
| --- | --- |
| 任意代码修改 | `docs/development/ai-guide.zh_CN.md`、相关头文件和相邻实现 |
| 应用工作流或核心技能配置 | `skills/README.zh_CN.md`；确认五个必需技能可用，再使用匹配当前任务的技能 |
| 环境引导或缺少工具链 | `docs/development/engineering/environment-setup.zh_CN.md` |
| BSP、引脚、总线、显示、音频、电池 | `docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md`、`components/bsp/include/bsp_pins.h` |
| Demo 或菜单 | `main/demo.h`、`main/main.c`、最近的 `main/demo_*.c` 实现 |
| 中文 UI 或字体 | `docs/development/engineering/lvgl-chinese-fonts.zh_CN.md`、应用字体素材、配置与控件样式 |
| Wi-Fi 联网或蓝牙配网 | `docs/development/engineering/wifi-provisioning.zh_CN.md`、其中引用的 `demo/blufi-provisioning` 实现 |
| 构建、测试、依赖、分区 | `docs/development/engineering/build-and-test.zh_CN.md`、`docs/development/engineering/firmware-layout.zh_CN.md`、`sdkconfig.defaults`、`partitions.csv` |
| CI 或发布 | `docs/development/ci/CI-*.zh_CN.md` 中的对应文件与 `.github/workflows/` |
| 项目开发完成 | `docs/development/release/project-completion.zh_CN.md`（再进入 `issue-suggestions` 或 `experience-pr` skill） |
| 文档 | `docs/contribution/doc-conventions.zh_CN.md`、`docs/README.zh_CN.md` |
| Commit 或 PR | `docs/contribution/commit-and-pr.zh_CN.md` |

产品概览与文档索引见 `docs/README.zh_CN.md`。详细的 AI 开发工作流（上下文建立、事实来源优先级、应用/BSP 边界、运行时规则、素材放置、交付格式）见 `docs/development/ai-guide.zh_CN.md`。Fork 专用流程见 `docs/fork-guide.zh_CN.md`，普通上游开发无需读取。

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

每次完整实现用户提出的固件需求后，必须主动询问是否将固件刷写到设备中进行
测试，不能只等到发布时才询问。未检测到设备时，提示用户将设备开机，再用
支持数据传输的数据线连接电脑 USB 接口。遵循
[真机测试交接流程](docs/development/ai-guide.zh_CN.md#主动询问真机测试)，
烧录前须取得用户同意；检测到设备本身不代表获得烧录授权。

在用户提出的改动范围内，按逻辑单元逐个提交，不必每次提交前询问；push 仅在用户
请求或当前工作流明确要求时执行。提交的单元粒度和 message 内容遵循
[提交与 PR 规范](docs/contribution/commit-and-pr.zh_CN.md#提交规范)。普通功能、应用和文档 PR 不得修改 `docs/CHANGELOG.md` 或 `docs/CHANGELOG.zh_CN.md`；用户可见行为、兼容性和发布流程影响改为写入 PR 正文及对应权威文档。发布准备期间，由发布负责人在创建 tag 前把已合并的用户可见变化统一汇总到两份变更日志。

社区规范见 `.github/CONTRIBUTING.zh_CN.md`、`.github/CODE_OF_CONDUCT.zh_CN.md`、`.github/SECURITY.zh_CN.md` 与 `.github/SUPPORT.zh_CN.md`。
