<p align="right">
  <strong>简体中文</strong> · <a href="agent-guide.md">English</a>
</p>

# AI Agent 开发指南

本指南面向 AI 编程助手。`AGENTS.md` 是唯一强制起点；代码任务再阅读本指南，并仅在任务需要时进入硬件或工程参考文档。

## 建立上下文

1. 阅读 `AGENTS.md` 并遵循任务路由，不要默认加载全部 README 或完整硬件指南。
2. 运行 `git status --short --branch` 并保留已有改动。
3. 阅读受影响的公共头文件、实现和相邻代码，不根据通用 ESP32-C3 开发板推断本板行为。
4. 把当前 checkout 作为完整实现来源，不获取或依赖远程 demo 分支。
5. 将需求拆成输入、输出、状态、任务、持久化、内存预算、失败行为和验收标准。缺少会改变可观察行为的决定时，必须在编辑前询问用户。
6. 迭代时运行聚焦检查，交付前运行 `./tools/validate.sh`，并明确保留硬件检查项。

## 事实来源优先级

```text
产品规格 / 实机测量
  > components/bsp/include/bsp_pins.h
  > BSP 公共头文件与实现
  > docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md
  > README 与 demo 应用
```

任务所需板卡版本、接线、极性、寄存器值或 GPIO 分配未在这些来源中定义时，直接询问用户。不得使用其它 ESP32-C3 开发板的参数替代。

## 应用与 BSP 边界

```text
需求
  └─ main/                         页面、状态机、动画、应用任务、素材
      └─ components/bsp/include/  稳定板级 API
          └─ components/bsp/src/  总线、设备与驱动细节
              └─ bsp_pins.h       引脚与硬件参数的唯一事实来源
```

新增页面时，在 `main/demo_<feature>.c` 中实现 `enter`、`exit` 和 `key` 接口，在 `main/demo.h` 声明、加入 `main/CMakeLists.txt`，并在 `main.c` 注册。每个新增硬件页面都必须在菜单中定义初始化失败状态。

只有可复用硬件能力进入 BSP。记录阻塞行为、任务上下文、所有权、失败值和初始化顺序。引脚与 I2C 地址只允许定义在 `bsp_pins.h`。

## 运行时不变量

- 非 LVGL 上下文访问 LVGL 对象时必须持有 `bsp_lvgl_lock()`。
- 按键回调只派发轻量事件；音频、存储、网络和其它慢操作放入工作任务。
- 删除页面 screen 前，停止代码路径中会访问该页面或其子对象的每个任务和定时器。
- 除非改动明确重新定义交互，否则保留菜单 `UP`/`DOWN`、`OK` 单击进入和页面 `OK` 长按返回。
- 未明确要求直接启动应用时必须保留菜单。明确要求直接启动时，从 `app_main` 加载目标页面；除非需求明确替换视觉系统，否则继续使用 `ui_pixel_screen_create()` / `ui_pixel_panel_create()`。
- 为图片、字体、网络、音频、LVGL 和任务栈预留内部 RAM；本板没有 PSRAM。
- 将可测试的状态机、协议、计时与布局计算从 ESP-IDF/LVGL 中分离，并用主机测试覆盖。

## 素材放置

应用自有的图片、字库和音频放在 `main/assets/` 下；只有同一次变更至少加入一个被跟踪素材时才创建该目录。记录来源与授权，并在 `main/CMakeLists.txt` 中登记每个嵌入或生成步骤。只有可复用板级行为实际消费的资源才进入 `components/bsp`。不要创建空素材分类，也不要把二进制素材与 Markdown 文档混放。

## 交付

自动门禁不等于硬件验收。分别报告 `Build`、`Host tests`、`Device tests` 和 `Unverified`。按[硬件指南](../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md)执行适用的实机验收矩阵。

相关文档：[构建与验证](build-and-test.zh_CN.md)、[代码约定](coding-conventions.zh_CN.md)、[硬件指南](../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md)和[文档索引](../INDEX.zh_CN.md)。
