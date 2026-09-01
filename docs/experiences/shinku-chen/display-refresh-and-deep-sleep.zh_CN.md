<p align="right">
  <strong>简体中文</strong> · <a href="display-refresh-and-deep-sleep.md">English</a>
</p>

# ESP32-C3（无 PSRAM）上的显示刷新与深睡

在「今天吃啥」单应用固件发布后沉淀。这些是通用、上游受益的经验，适用于任何
AI Passport 应用，而非 fork 专属定制。

> **验证状态。** “ESP32-C3 的深睡 GPIO 唤醒”一节已在本项目「音效钥匙扣」固件上真机验证
> （提交 `63f0ebd`）：空闲 5 分钟入睡，任意按键可唤醒重进应用。其余事实（LCD 字节序、
> LVGL 崩溃特征）仍来自本项目自身观察，尚未附上固定来源 commit/tag、确切测试板版本或
> 复现实测，请视为未经验证的现场笔记。

## 直接刷新单个图片矩形

对于只需每帧重绘固定图片区域的 UI，绕过 LVGL 的局部重绘路径、直接驱动 LCD
面板更可控：

- `bsp_display_panel()` 暴露 `esp_lcd_panel_handle_t`；用
  `esp_lcd_panel_draw_bitmap(panel, x1, y1, x2, y2, color_data)` **只推图片矩形**。
- panel 层**不做**字节交换（swap 是 `esp_lvgl_port` 的 flag），故 ST7789 SPI 路径
  要推**大端 RGB565**。
- **本项目生成的** LVGL I8 索引位图在像素索引字节前内嵌 256×4 (B,G,R,A) 调色板；
  用小的行缓冲逐行解码为 RGB565，避免在无 PSRAM 部件上占整帧 RAM。该调色板布局是
  本项目 LVGL I8 的约定，并非所有 I8 数据的通用规则。
- 可选隔行两趟（先偶行后奇行）让每趟 SPI 像素减半，代价是可见撕裂——适合 A/B
  对比速度与撕裂。
- 让图片矩形的原点 (x, y) 在 LVGL 布局与直画之间共享，保证两条路径一致。

## ESP32-C3 的深睡 GPIO 唤醒

ESP32-C3 **没有 EXT0/EXT1** 唤醒。用 `esp_deep_sleep_enable_gpio_wakeup()` 在
RTC GPIO 上唤醒。本板按键 ADC 节点是 GPIO0，带外部 10k 上拉（常态高），故**低电平**
唤醒（`ESP_GPIO_WAKEUP_GPIO_LOW`）在任意按键按下时触发。

> **坑（已在真机验证）。** `esp_deep_sleep_enable_gpio_wakeup()` 的第一个参数是
> **引脚位掩码**，不是引脚号。若传 `GPIO_NUM_0`（值 0），函数把 `gpio_pin_mask == 0`
> 视为非法，直接返回 `ESP_ERR_INVALID_ARG`，**什么唤醒源都不会配**；再忽略返回值
> 调用 `esp_deep_sleep_start()`，就是一次没有任何唤醒源的深睡——按键拉低 GPIO0 永远
> 无法唤醒（表现为“睡了按不醒”）。必须传 `1ULL << GPIO_NUM_0`，并对返回值做判断 +
> 日志，失败时打日志而不是静默睡死。排查“睡了按不醒”时，先确认唤醒源是否真的配上了。

- 顺序：先用 `gpio_config()` 把该 GPIO 配成数字输入，再
  `esp_deep_sleep_enable_gpio_wakeup(1ULL << GPIO_NUM_0, ESP_GPIO_WAKEUP_GPIO_LOW)`，
  最后 `esp_deep_sleep_start()`。
- `esp_deep_sleep_start()` 不返回；唤醒后应用冷启动，需重新初始化休眠所替换的
  外设与 ADC 按键句柄。ESP32-C3 深睡会让 USB-Serial/JTAG 控制台失效（端口操作报
  “设备不识别此命令”）；重开机后控制台恢复，启动日志会打印 `esp_sleep_get_wakeup_cause()`
  的唤醒原因——这就是在真机确认 GPIO 唤醒的方法。
- 注意 ESP-IDF 5.5.3 默认会启用 RTC GPIO 内部电阻
  （`CONFIG_ESP_SLEEP_GPIO_ENABLE_INTERNAL_RESISTORS=y`），会在外部 10 kΩ 上拉之上
  叠加内部上拉。若意图是**只**依靠外部上拉，需显式关闭该配置——此行为尚未在真机验证。
- 仅定时器唤醒会造成周期性重启，而非真正的“关机”；意图是睡到用户操作时应使用
  GPIO 唤醒。

## LVGL 对象类型误用及其崩溃特征

把非 label 的 `lv_obj_t*`（如 panel）传给 `lv_label_set_text()` 会破坏堆：
LVGL 把该对象当作 label，释放一个垃圾文本指针，在 `lv_tlsf_block_size` 中出错——
`Load access fault`，`MTVAL` 接近一个小偏移（如 `0x68`），并循环重启。应在专用变量里
保存 `lv_label_create()` 真正创建的 `lv_obj_t*` 并使用它，而不是容器。
