<p align="right">
  <strong>简体中文</strong> · <a href="coding-conventions.md">English</a>
</p>

# 代码约定

## C 与命名

- 使用四空格缩进和 K&R 大括号。
- 函数与变量使用 `snake_case`，文件内状态使用 `s_`，公开板级常量使用 `BSP_*`，公开 BSP API 使用 `bsp_`，demo 入口使用 `demo_<feature>_<action>`。
- 除非其它 translation unit 实际使用，否则每个符号都声明为 `static`。
- UI 字符串使用英文。源码注释可以使用中文，但保留既有英文技术标识符。

## 所有权与运行时

- 可复用板级行为、总线、驱动和固件常量放在 `components/bsp`；页面、应用状态、动画和应用任务放在 `main`。
- 每个非 LVGL 任务或回调访问 LVGL 对象前必须调用 `bsp_lvgl_lock()`，每条成功加锁路径都必须解锁。
- 按键回调只能入队或发送通知。音频 PCM、存储、网络、内存分配和等待必须在工作任务运行。
- 页面退出时，必须先停止并等待所有可访问该页面的任务、timer、回调和事件源退出，再删除 screen。
- 每个 LVGL 指针必须保持构造函数返回的对象类型。传给 `lv_label_set_text()` 的指针必须来自 `lv_label_create()`。

## UI、字体与功耗

- 除非需求明确替换视觉系统，否则保留现有 `ui_pixel`。保留主题的页面使用 `ui_pixel_screen_create()` 和 `ui_pixel_panel_create()`。
- 基线只启用 Montserrat 14 和 20，两者都不含 CJK glyph。增加 CJK UI 文本的同一次变更必须加入覆盖全部显示字符的 glyph 子集字体、配置混排 fallback、报告 Flash/RAM 成本，并包含实机字形验收。
- 每个产品应用需求必须定义空闲超时、背光行为和 light/deep sleep 行为。缺少任一项时，必须在实现功耗行为前询问用户。

## 注释、数据与资源

- 注释不明显的所有权、阻塞、任务上下文、同步、失败值、寄存器选择、时序和硬件常量。语句本身已经完整表达含义时不重复注释。
- 每个设备持久化格式在发布前必须版本化。修改格式时，必须在写代码前定义迁移方案或取得清除数据的明确授权。
- 只有同一次变更至少加入一个被跟踪素材时才创建 `main/assets/`。记录来源与许可证，登记每个嵌入/生成步骤，并报告 Flash 与内部 RAM 影响。
- ESP32-C3 无 PSRAM。增加 LVGL buffer、音频 buffer、无线状态或任务栈时，必须附带构建内存报告，以及实机最小空闲堆和最大连续块测量。

## 测试

- 修改脱离硬件的状态机、协议、计时规则、持久化 codec 或布局计算时，必须在同一次变更新增或更新 `tests/test_*.c`。
- 仅硬件修改必须在 PR 中准确列出硬件指南的验收行，并把每项报告为 `PASS`、`FAIL` 或 `NOT RUN`。
- 迭代时运行 `./tools/run-host-tests.sh`，交付前运行 `./tools/validate.sh`。
