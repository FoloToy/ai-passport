# AI Agent 开发指南（Agent Development Guide）

> 定位：面向 AI 编程助手（Claude Code / Codex / Cursor / Cline 等），人类开发者可忽略。
> 本文档集中说明"AI 如何在本仓库工作"，收纳了原 `docs/README` 中的 AI 使用说明，并链接到各权威文档，避免重复。触发场景：AI 开始任何开发工作前，先读 `AGENTS.md`（规范入口与规则索引），再按需读本文档与硬件指南。

## 0. 与其他文档的关系

| 文档 | 作用 | 本文档的取舍 |
| --- | --- | --- |
| [AGENTS.md](../../AGENTS.md) | AI 规范入口 + 按触发场景的规则索引 | 必读；本文档不重复规则 |
| [AI 硬件开发指南](../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md) | 硬件事实、引脚表、验收矩阵、故障速查 | 硬件细节权威源；涉及硬件结论时以其为准 |
| [build-and-test.md](build-and-test.md) | 构建与验证命令 | 本文档不重复命令，只引用 |
| [docs/README.md](../README.md) | 人读的项目总览 | 已不含 AI 说明 |

## 1. 开始开发前：建立上下文

开始开发前，按以下顺序建立上下文：

1. 阅读 `AGENTS.md`、[docs/README.md](../README.md) 和 [docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md](../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md)。
2. 执行 `git status --short --branch`，保留用户已有改动。
3. 阅读需求会触及的 `components/bsp/include/*.h` 及其实现，不根据芯片或开发板的常见配置猜测本板行为。
4. 用 `git branch -r --list 'origin/demo/*'` 查找接近需求的示例，只复用相关设计，不默认合并整个示例分支。
5. 将需求拆成输入、输出、状态、并发任务、持久化、内存预算和失败降级，再决定修改 `main` 还是扩展 `components/bsp`。
6. 完成最低构建检查和适用的逻辑测试；所有依赖屏幕、按键、音频、电池或时序的结论均保留真机验收项。

## 2. 事实来源优先级（Source-of-truth priority）

发生冲突时，使用以下优先级：

```text
原理图 / PCB / 板卡版本 / 实机测量
    > components/bsp/include/bsp_pins.h
    > BSP 公开头文件与实现
    > docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md
    > README 与示例应用
```

当前仓库尚未包含原理图和 PCB 源文件。遇到板卡版本、接线、极性、寄存器或未使用 GPIO 等未知信息时，明确报告未知项并请求证据，不能用其他 ESP32-C3 开发板的参数补全答案。

> 硬件指南有同义说明；以本文档与硬件指南为准，README 不再重复。

## 3. 向 agent 提出需求

简单需求可以直接这样交给 agent：

```text
On the main branch, build an offline habit-tracking application for FoloToy AI Passport.
Use the three physical buttons and the 240×320 display, and preserve records across power loss.
Follow AGENTS.md and docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md. Inspect relevant demo branches first,
keep hardware logic in components/bsp and application logic in main, deliver a runnable
implementation with tests, and report the build result, unexecuted device checks, and exact
on-device acceptance steps separately.
```

需求越具体，agent 越容易一次实现正确。建议说明：

- 用户流程：每个页面显示什么，三个按键的短按、双击、长按分别做什么；
- 状态与数据：是否计时、断电保存、联网、录音或与电脑通信；
- 体验目标：字体、颜色、动画、声音、响应时间和异常状态；
- 限制条件：是否允许替换主菜单、增加依赖、使用 Flash 或改变默认交互；
- 验收标准：哪些行为必须自动测试，哪些必须在真实硬件观察。

若需求没有给出所有细节，agent 可以在不改变产品方向的范围内采用保守默认值，但应在交付中列出这些假设。涉及新接线、电源安全、硬件版本或不可恢复数据格式的决定必须先确认。

## 4. 应用与 BSP 的边界

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

## 5. 运行时不可破坏的规则（Runtime invariants）

- LVGL 不是线程安全的；非 LVGL 上下文操作 `lv_*` 对象必须持有 `bsp_lvgl_lock()`。
- 按键回调只派发轻量事件；录音、播放、存储和其他慢操作放到工作任务。
- 页面退出时先停止可能访问 UI 的任务或定时器，再删除 screen 并清空对象指针。
- 全局交互默认是菜单中 `UP`/`DOWN` 导航、`OK` 单击进入、页面中 `OK` 长按返回；改动时要明确说明。
- 新图片、字体、网络栈、音频缓存、LVGL buffer 或任务栈都要评估内部 RAM；总空闲堆足够不代表存在足够大的连续内存块。
- 可测试的状态机、协议、计时和布局计算应与 ESP-IDF/LVGL 分离，优先加入主机逻辑测试。

## 6. 验收与交付格式

`idf.py build` 是最低自动检查，不是硬件验收。agent 的最终交付应明确区分：

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
