<p align="right">
  <strong>简体中文</strong> · <a href="fap-screenshot-without-frame-buffer.md">English</a>
</p>

# 在没有多余帧缓冲的板子上实现 `FAP_SCREENSHOT_V1`

本条目来自 **limelight lemonade jam 阅读器**（`v0.1.0-limelight`，提交 `dc7154d`），
设备是无 PSRAM 的 AI Passport。下面的数字都是在该板上实测的；固件已通过社区 publisher 的
验收，投稿用的 PNG 与回执就是用它自己的抓屏工具产出的。

## 为什么必须实现这个协议

社区上架要求提供一段新鲜的串口抓屏作为游玩证据，只打日志的固件会被拒绝。它的工具以 115200 打
开串口、写入 `FAP_SCREENSHOT_V1\n`，然后期待一行 ASCII 头部 + 紧跟其后的声明长度负载：

```text
FAP_SCREENSHOT_V1 <width> <height> RGB565LE|PNG <byte_length>\n
<binary payload>
```

工具的三条行为决定了实现方式：它**精确**读取 `byte_length` 个字节、整个过程只给 **15 秒**、
成功后写一份 `.fap-capture.json` 回执（`validate` / `submit` 认 24 小时内的）。

## 约束：凑不出 150KB 的帧缓冲

最直观的写法是一块静态 240 × 320 × 2 = 153,600 字节帧缓冲加 `lv_snapshot_take_to_draw_buf()`。
这块板子拿不出来：阅读器已经占着 240 × 214 画面区画布（102,720 B）、40KB 立绘解码缓冲、
20.5KB 剧本块缓存、3.2KB 遮罩缓冲和 28KB LVGL 池，启动时最大空闲块只有约 9KB。

出路是复用画面区画布、**分两段抓**——协议只约束流的字节顺序，不约束发送方怎么产出：

1. 先播报 `240 320 RGB565LE 153600`；
2. 把 `[0, 214)` 行抓进画布缓冲，写出这 102,720 字节；
3. 把 `[214, 320)` 行抓进同一块缓冲（第二段会把行折叠到缓冲开头），写出剩下的 50,880 字节。

合计正好是声明的长度，不需要第二块缓冲，而且复用了现有的抓帧路径（一个 LVGL flush 钩子，把被
刷新的行抄进画布）。

## 四个花了真实时间的坑

**抓帧缓冲同时就是屏上画布。** 抓帧前必须清空它（LVGL 是分块刷新的，不清会残留上一帧的行），
但清空会让背景与立绘变黑——回传的帧**和屏幕本身**都是黑的。修法：清完立刻重画，再失效+刷新：

```c
memset(s_art_pixels, 0, sizeof(s_art_pixels));
lime_app_debug_redraw_art(&s_app);   /* 强制重画背景/立绘 */
s_capture_row_offset = row_offset;
s_capture_frame = true;
lv_obj_invalidate(lv_screen_active());
lv_refr_now(NULL);
```

**控制台默认路径慢得离谱。** 走寄存器级 USB-Serial-JTAG VFS 实测只有 **2.7 KB/s**：一整帧要
**56 秒**，于是 publisher 的 15 秒预算用尽，报 "device screenshot data ended before the
declared length"。装上驱动并把 VFS 切过去之后，同样的传输降到 **1.66 秒**：

```c
usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
cfg.tx_buffer_size = 1024;   /* 参考实现用的就是这个量级 */
cfg.rx_buffer_size = 256;
usb_serial_jtag_driver_install(&cfg);
usb_serial_jtag_vfs_use_driver();
```

**短写会静默丢整块。** 用 `fwrite()` 时一次传输正好丢了 2048 字节的一块，之后的像素全部错位，
但出来的图仍然像一张正常的图。必须检查返回值并重试；改成 `usb_serial_jtag_write_bytes()` 按
512 字节分块 + 停滞计数就够了。

**负载里混进一个日志字节，整张图就错位。** 控制台与负载共用同一条 USB-CDC 流，而上位机按精确
字节数读取，任何落在传输中间的 `ESP_LOG` 都会毁掉这一帧。传输期间静音日志
（`esp_log_level_set("*", ESP_LOG_NONE)`），结束后恢复。

## 驱动的两条摆放规则

- **不要放在 `app_main()` 开头。** 那样写实测设备**没有任何输出**、只能重新刷写；VFS 必须已经
  建好。正确位置是应用初始化完成之后、从 VFS 读命令的任务启动之前。
- **先建大栈任务，再装驱动。** 驱动缓冲从同一个堆里拿：在装完驱动之后再建 8KB 栈的输入任务会
  失败（“输入任务创建失败”），阅读器直接起不来——尽管当时还剩 17KB 空闲。反过来先建任务、
  再用 1KB/256B 的缓冲装驱动，启动后剩 1.8KB 空闲堆即可正常工作。

## 怎么验证抓屏

`capture-screen --port <口> --output capture.png` 会写出 PNG 和回执。如果工具报"负载比声明的
短"，先量固件侧：写一个小脚本自己跑一遍协议，打印收到多少字节、花了多久——这样能把"路径太慢"
（几十秒）和"中途截断"分开。
