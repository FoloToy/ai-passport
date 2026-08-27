<p align="right">
  <strong>简体中文</strong> · <a href="agent-guide.md">English</a>
</p>

# AI Agent 开发指南（Agent Development Guide）

> 定位：面向 AI 编程助手（Claude Code / Codex / Cursor / Cline 等），人类开发者可忽略。
> 本文档集中说明“AI 如何在本仓库工作”。所有任务只强制先读 `AGENTS.md`；涉及代码开发时再读本文档，并按路由加载相关硬件或工程说明。

## 0. 与其他文档的关系

| 文档 | 作用 | 本文档的取舍 |
| --- | --- | --- |
| [AGENTS.md](../../AGENTS.md) | AI 规范入口 + 按触发场景的规则索引 | 必读；本文档不重复规则 |
| [AI 硬件开发指南](../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md) | 硬件事实、引脚表、验收矩阵、故障速查 | 仅硬件相关任务必读；涉及硬件结论时以其为准 |
| [build-and-test.md](build-and-test.md) | 构建与验证命令 | 本文档不重复命令，只引用 |
| [docs/README.md](../README.md) | 人读的项目总览 | 已不含 AI 说明 |

## 1. 开始开发前：建立上下文

开始开发前，按以下顺序建立上下文：

1. 阅读 `AGENTS.md`，根据其中的任务路由只加载当前修改所需文档；不要默认读取全部 README 或完整硬件指南。
2. 执行 `git status --short --branch`，保留用户已有改动。
3. 阅读需求会触及的 `components/bsp/include/*.h` 及其实现，不根据芯片或开发板的常见配置猜测本板行为。
4. 用 `git branch -r --list 'origin/demo/*'` 查找接近需求的示例，只复用相关设计，不默认合并整个示例分支。
5. 将需求拆成输入、输出、状态、并发任务、持久化、内存预算和失败降级，再决定修改 `main` 还是扩展 `components/bsp`。
6. 迭代时运行最小相关测试，交付前运行 `./tools/validate.sh`；所有依赖屏幕、按键、音频、电池或时序的结论均保留真机验收项。

## 2. 事实来源优先级（Source-of-truth priority）

发生冲突时，使用以下优先级：

```text
产品规格 / 实机测量
    > components/bsp/include/bsp_pins.h
    > BSP 公开头文件与实现
    > docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md
    > README 与示例应用
```

任务所需板卡版本、接线、极性、寄存器或 GPIO 分配未在这些来源中定义时，直接询问用户，不能用其他 ESP32-C3 开发板的参数补全答案。

> 硬件指南有同义说明；以本文档与硬件指南为准，README 不再重复。

## 3. 应用与 BSP 的边界

```text
Natural-language requirement
  └─ main/                         Pages, state machines, animation, app tasks, assets
      └─ components/bsp/include/  Stable board-level APIs
          └─ components/bsp/src/  GPIO, buses, devices, and driver details
              └─ bsp_pins.h       Single source of truth for pins and hardware parameters
```

新增普通页面时，创建 `main/demo_<feature>.c` 并实现 `enter`、`exit`、`key` 接口，然后同步修改：

- `main/demo.h` 中的声明；
- `main/CMakeLists.txt` 中的源文件；
- `main/main.c` 的 `DEMOS[]` 注册；
- 若有新的可选外设，菜单的初始化状态与失败降级。

只有多个应用都会使用的硬件能力才进入 `components/bsp`。BSP API 需要说明阻塞性、线程上下文、内存所有权、失败值和初始化顺序；引脚或 I2C 地址只能加入 `bsp_pins.h`。

## 4. 运行时不可破坏的规则（Runtime invariants）

- LVGL 不是线程安全的；非 LVGL 上下文操作 `lv_*` 对象必须持有 `bsp_lvgl_lock()`。
- 按键回调只派发轻量事件；录音、播放、存储和其他慢操作放到工作任务。
- 页面退出时先停止可能访问 UI 的任务或定时器，再删除 screen 并清空对象指针。
- 全局交互默认是菜单中 `UP`/`DOWN` 导航、`OK` 单击进入、页面中 `OK` 长按返回；改动时要明确说明。
- 当用户要求删除不必要组件或直接进入某功能时，可以去掉主界面及其元素、直接载入目标功能界面；但必须**保留 `ui_pixel` 主题体系**（天空底色、草地、标题牌、吉祥物、墨色描边面板），不要把它当成"不必要组件"删掉，否则界面会渲染成空白。复用 `ui_pixel_screen_create()` / `ui_pixel_panel_create()`，让功能页保持统一的视觉身份。
- 默认情况下，在用户界面的**右上角显示电量信息**（读取 `bsp_battery_soc()`），除非开发者明确指定其它位置或明确不需要。读值为 `-1`（不可用）时优雅降级，而不是画一个数字。位置要避开右上角已有的**白云装饰**（`add_cloud`，约 `x≈188, y≈8`），用空闲的蓝天区，不要盖住白云。
- 新图片、字体、网络栈、音频缓存、LVGL buffer 或任务栈都要评估内部 RAM；总空闲堆足够不代表存在足够大的连续内存块。
- 可测试的状态机、协议、计时和布局计算应与 ESP-IDF/LVGL 分离，优先加入主机逻辑测试。

## 5. 素材放置（Material placement）

当开发者通过你提交可复用素材（图片、字库、音频或类似的工程素材）时，默认保存到仓库根目录 [`assets/`](../../assets/README.md)，以便开发及后续复用。将其放入对应的子目录（`assets/images/`、`assets/fonts/`、`assets/music/`），并在该子目录的 `README.md` 中记录放置路径、命名规则、集成方式与来源/授权。二进制素材不得与 Markdown 文档混放。应用或经验归档记录（封面、手册、摘要）属于 `plays/` 或 `docs/experiences/`，不放入 `assets/`；除非开发者明确指定其它位置，否则不要偏离 `assets/`。

## 6. 验收与交付格式

`./tools/validate.sh` 是完整自动门禁，但不是硬件验收。agent 的最终交付应明确区分：

```text
Build: PASS / FAIL / NOT RUN
Host tests: PASS / FAIL / NOT RUN
Device tests: PASS / FAIL / NOT RUN
Unverified: 仍需板卡、仪器或用户确认的事项
```

上板验收矩阵按修改类型（引脚、LCD、ADC、codec、I2C、DMA 等）见 [AI 硬件开发指南 §构建与验证](../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md#构建与验证)，本文档不重复完整验收清单。真机结果要与"编译通过"分开记录。

## 7. 相关文档

- 构建与验证命令：[build-and-test.md](build-and-test.md)
- 代码约定：[coding-conventions.md](coding-conventions.md)
- 硬件指南与验收矩阵：[AI 硬件开发指南](../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md)
- 全部文档索引：[docs/INDEX.md](../INDEX.md)
