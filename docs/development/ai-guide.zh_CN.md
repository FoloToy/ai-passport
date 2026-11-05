<p align="right">
  <strong>简体中文</strong> · <a href="ai-guide.md">English</a>
</p>

# AI Agent 开发指南（AI Agent Development Guide）

> 定位：面向 AI 编程助手（Claude Code / Codex / Cursor / Cline 等），人类开发者可忽略。
> 本文档集中说明“AI 如何在本仓库工作”。所有任务只强制先读 `AGENTS.zh_CN.md`；涉及代码开发时再读本文档，并按路由加载相关硬件或工程说明。

## 1. 开始开发前：建立上下文

开始开发前，按以下顺序建立上下文：

1. 阅读 `AGENTS.zh_CN.md`，根据其中的任务路由只加载当前修改所需文档；不要默认读取全部 README 或完整硬件指南。
2. 执行 `git status --short --branch`，保留用户已有改动。按 [AGENTS.zh_CN.md](../../AGENTS.zh_CN.md#必需-ai-技能) 确认五个必需技能可用；缺少时由 AI 自行选择方式并完成安装，不把安装准备交给用户。
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

## 3. 应用与 BSP 的边界

```text
Natural-language requirement
  └─ main/                         Pages, state machines, animation, app tasks, assets
      └─ components/bsp/include/  Stable board-level APIs
          └─ components/bsp/src/  GPIO, buses, devices, and driver details
              └─ bsp_pins.h       Single source of truth for pins and hardware parameters
```

维护基线硬件测试 demo、添加测试页面时，创建 `main/demo_<feature>.c` 并实现 `enter`、`exit`、`key` 接口，然后同步修改：

- `main/demo.h` 中的声明；
- `main/CMakeLists.txt` 中的源文件；
- `main/main.c` 的 `DEMOS[]` 注册；
- 若有新的可选外设，菜单的初始化状态与失败降级。

上述注册方式仅用于基线测试 demo，不是二次开发应用必须采用的 UI 结构。

只有多个应用都会使用的硬件能力才进入 `components/bsp`。BSP API 需要说明阻塞性、线程上下文、内存所有权、失败值和初始化顺序；引脚或 I2C 地址只能加入 `bsp_pins.h`。

### 二次开发 UI 强制重新设计

所有二次开发应用都必须围绕自身需求，重新设计并实现页面、布局、视觉呈现、
导航流程和按键交互。这是交付要求，不是默认主题建议。

- 禁止将基线 `main` 测试菜单、`demo_*.c` 测试页面或现有 `ui_pixel` demo
  界面外壳作为应用 UI，也不能复制到改名文件后继续使用。仅改文字、换颜色、
  隐藏测试入口，或在原测试外壳上增加功能页，都不算完成重新设计。
- 可以复用 BSP 驱动／API、普通 LVGL 控件、独立逻辑，以及适用的生命周期／
  并发模式。重新设计 UI 不要求重写驱动，也不禁止像素风等通用视觉风格。
- 宣布 UI 实现完成前，检查启动与导航路径，确认进入的是重新设计的应用页面，
  并在交付中说明新页面和按键交互。即使编译通过，沿用测试 UI 仍属于未完成；
  实际屏幕显示须单独记录真机验证结果。

仅在任务本身是维护基线硬件测试 demo 时，可以保留其测试 UI；这不属于二次
开发应用。本规则不要求从基线仓库删除参考 demo。

## 4. 运行时不可破坏的规则（Runtime invariants）

- LVGL 不是线程安全的；非 LVGL 上下文操作 `lv_*` 对象必须持有 `bsp_lvgl_lock()`。
- 按键回调只派发轻量事件；录音、播放、存储和其他慢操作放到工作任务。
- 页面退出时先停止可能访问 UI 的任务或定时器，再删除 screen 并清空对象指针。
- 维护基线 demo 时，除非任务明确修改交互，否则保留菜单中 `UP`/`DOWN` 导航、`OK` 单击进入、页面中 `OK` 长按返回。二次开发应用按上面的强制 UI 重新设计规则，自行定义按键和导航。
- 只有继续留在基线 demo 中的硬件验证页才使用 `ui_pixel_screen_create()` / `ui_pixel_panel_create()` 保持该 demo 的视觉一致；不得把测试外壳带入二次开发应用。
- 默认情况下，在用户界面的右上角显示电量信息（读取 `bsp_battery_soc()`），除非开发者指定其他位置或明确不需要。读值为 `-1`（不可用）时优雅降级。避免遮挡应用自己的内容；维护基线 demo 时，还需避开白云装饰（`add_cloud`，约 `x≈188, y≈8`）。
- 新图片、字体、网络栈、音频缓存、LVGL buffer 或任务栈都要评估内部 RAM；总空闲堆足够不代表存在足够大的连续内存块。
- 中文显示属于字体集成任务，不只是翻译字符串。修改文案、字体、字号、主题或动态内容时，必须执行[中文字体检查清单](engineering/coding-conventions.zh_CN.md#中文字体与缺字排查)；不得通过隐藏缺字方框来假装修复显示问题。
- 可测试的状态机、协议、计时和布局计算应与 ESP-IDF/LVGL 分离，优先加入主机逻辑测试。

## 5. 素材放置（Material placement）

当开发者通过你提交可复用素材（图片、字库、音频或类似的工程素材）时，默认保存到仓库根目录 [`assets/`](../../assets/README.zh_CN.md)，以便开发及后续复用。将其放入对应的子目录（`assets/images/`、`assets/fonts/`、`assets/music/`），并在 [`assets/` README](../../assets/README.zh_CN.md) 中记录放置路径、命名规则、集成方式与来源/授权。二进制素材不得与 Markdown 文档混放。纯文本应用档案（封面元数据、手册、摘要）放在相对仓库根目录的 `docs/reference/<username>/<app-name>/`，经验条目放在 `docs/reference/<username>/`。这些记录不放入 `assets/`，也不把封面图片提交到档案中。可复用素材仍默认放在 `assets/`，除非开发者明确指定其它位置。

## 6. 验收与交付格式

`./tools/validate.sh` 是完整自动门禁，但不是硬件验收。agent 的最终交付应明确区分：

```text
Build: PASS / FAIL / NOT RUN
Host tests: PASS / FAIL / NOT RUN
Device tests: PASS / FAIL / NOT RUN
Unverified: 仍需板卡、仪器或用户确认的事项
```

上板验收矩阵按修改类型（引脚、LCD、ADC、codec、I2C、DMA 等）见 [AI 硬件开发指南 §构建与验证](../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md#构建与验证)，本文档不重复完整验收清单。真机结果要与"编译通过"分开记录。

### 主动询问真机测试

每次完整实现用户提出的固件功能或修复需求，并执行相应验证后，必须主动询问
用户是否将本次固件刷写到设备中进行测试。该要求适用于每轮完整的开发迭代，
不只适用于项目发布；交接时不能只给出编译结果或固件路径。例如：

> 本次需求已完整实现，是否现在将验证通过的固件刷写到设备中进行测试？

1. 当前环境具备设备访问能力时，在交接阶段只读检查 USB／串口设备。不能仅因
   设备已连接就打开或复位任意串口，更不能自动烧录。如果存在多个候选设备，
   或不能确定目标，先请用户确认要测试的设备和端口。
2. 未检测到设备时，明确提示：

   > 暂未检测到设备。请先将设备开机，再使用支持数据传输的数据线（不能是
   > 仅充电线）连接电脑的 USB 接口。连接好后告诉我，我会重新检测。

   用户连接后重新检测。若当前环境无法访问用户电脑的 USB 设备，应说明访问
   能力限制，不能声称设备没有连接；改为指导用户在本机检测、烧录，并反馈
   测试结果。
3. 写入前确认目标设备、本次验证通过的具体固件及其
   [烧录／数据影响](engineering/firmware-layout.zh_CN.md#烧录与已存数据)，
   并取得本次烧录的明确同意。设备已连接、以前同意过其他版本烧录，都不构成
   本次授权。不得把备份设备原有固件作为前置条件，也不得默认获得全片擦除
   授权。
4. 经同意烧录后，按对应硬件验收清单检查启动和用户要求的功能，必要时请用户
   观察实机并反馈。仅烧录成功不能报告 `Device tests: PASS`。用户拒绝或暂缓
   测试、没有可用设备或无法访问设备时，报告 `Device tests: NOT RUN`，并在
   `Unverified` 中列出待验证项目。

纯文档等不改变固件的任务，应说明真机测试不适用，不要为满足流程而烧录无关
固件。邀请真机测试不代表获得 commit、push 或任意可选
[项目收尾动作](release/project-completion.zh_CN.md)的授权。

## 7. 相关文档

- 构建与验证命令：[build-and-test.zh_CN.md](engineering/build-and-test.zh_CN.md)
- 代码约定：[coding-conventions.zh_CN.md](engineering/coding-conventions.zh_CN.md)
- 硬件指南与验收矩阵：[AI 硬件开发指南](../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md)
- 全部文档索引：[docs/README.zh_CN.md](../README.zh_CN.md)
