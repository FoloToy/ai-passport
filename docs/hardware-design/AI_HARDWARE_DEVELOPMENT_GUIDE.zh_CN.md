<p align="right">
  <strong>简体中文</strong> · <a href="AI_HARDWARE_DEVELOPMENT_GUIDE.md">English</a>
</p>

# FoloToy AI Passport 硬件开发指南

本指南定义每台标准量产 AI Passport 的固件可见硬件。事实优先级依次为产品规格与实测结果、`bsp_pins.h`、BSP 头文件与实现、本指南、demo 代码。使用这些来源没有定义的硬件细节前必须询问用户。

硬件与代码事实于 2026-08-28 完成对齐。

## 板卡与引脚契约

目标平台为 ESP32-C3、8 MB Flash、无 PSRAM、ESP-IDF 5.5.3。

| GPIO | 量产功能 | 电气或所有权规则 |
| ---: | --- | --- |
| 0 | UP/DOWN/OK ADC 电阻梯；light/deep sleep 唤醒 | 外部 10 kΩ 上拉到 3.3 V；低电平唤醒；ADC1_CH0 只有一个 BSP 所有者 |
| 1 | LCD CS | SPI2 上的 ST7789P3 |
| 2 | I2S DOUT | MCU 到 ES8311 |
| 3 | I2S WS | MCU 为 I2S master |
| 4 | I2S DIN | ES8311 到 MCU |
| 5 | I2S BCLK | I2S0 TX/RX 共用 |
| 6 | I2S MCLK | 采样率的 256 倍时钟 |
| 7 | I2C SCL | I2C0；2.2 kΩ 外部上拉到 3.3 V |
| 8 | LCD SCLK | SPI2，40 MHz，mode 0 |
| 9 | LCD MOSI | 不存在 LCD MISO |
| 10 | I2C SDA | I2C0；2.2 kΩ 外部上拉到 3.3 V |
| 18/19 | USB Serial/JTAG | 保留给控制台与烧录 |
| 20 | LCD DC | ST7789P3 命令/数据选择 |
| 21 | LCD 背光 | LEDC low-speed timer 0/channel 0，5 kHz，10 bit；与默认 UART0 TX 冲突 |

LCD 复位未连接 MCU，使用控制器软件复位。扬声器功放没有 MCU 使能脚，直接由电池供电，并使用 4.2 V 作为 `esp_codec_dev` 的固定满电增益校准值。表中没有出现的 GPIO 不是公开应用接口。

独立电源键完全属于外部电源电路，固件无法读取。持续按住 0.5 秒开机，持续按住 2 秒关机。被动 NTAG213 不连接 MCU，也没有 BSP API。

## 资源所有权与生命周期

```text
app_main
  -> 初始化并扫描 BSP 持有的 I2C0
  -> 初始化 ST7789P3、LVGL 与背光
  -> 初始化 ADC 按键、ES8311 与 CW2017
  -> 加载七页面 LVGL 菜单
```

| 资源 | 所有者 | 规则 |
| --- | --- | --- |
| SPI2 | 显示 BSP | ST7789P3 专用；没有 MISO |
| ADC1_CH0 | 按键 BSP | 按键识别、实时电压与唤醒准备共用一个 unit |
| I2C0 | `bsp_i2c` | ES8311 与 CW2017 客户端复用一个 bus handle |
| I2S0 | 音频 BSP | TX/RX 共用 MCLK、BCLK 与 WS |
| Wi-Fi/BLE stack | 当前 demo 页面 | 进入时启动、退出时反初始化；基线中不同时运行 |
| NVS/netif/event loop | `demo_radio.c` | 只初始化一次；初始化错误时禁止清除无关 NVS |
| LVGL 对象 | LVGL 任务 | 其它任务或回调统一使用 `bsp_lvgl_lock()` |

显示与 LVGL 是菜单的硬依赖。按键、音频和 CW2017 初始化失败时对应页面标记 `[FAIL]`；三个器件仍是量产必装项。页面删除 screen 前，必须停止所有可访问该 screen 的任务、timer、回调和事件源。

## 显示与 LVGL

面板为 ST7789P3，240 × 320 竖屏 RGB565，SPI2 MOSI-only，40 MHz，mode 0。复位只使用软件路径，gap 为 `(0, 0)`，X/Y mirror 关闭，反色开启。`bsp_display.c` 中的 porch、power 和 gamma 命令属于量产面板，不得复用于其它面板。

LVGL 使用一个 `240 × 20` RGB565 DMA buffer，共 9,600 bytes。LVGL 内部池为 24 KiB。必须设置 `swap_bytes=true`，因为 LVGL 提供小端 RGB565，LCD SPI 数据流先发送高字节。基线字体是 Montserrat 14 与 20，不包含 CJK glyph。

应用页面使用 LVGL。只有 PR 包含实机测量并验证以下四项时才允许直接调用 `esp_lcd_panel_draw_bitmap()`：并发 LVGL 绘制已停止、持有显示/LVGL 生命周期锁、每个矩形都不越界、RGB565 使用 LCD 字节序。

## ADC 按键与休眠唤醒

GPIO0 通过 10 kΩ 上拉到 3.3 V。UP、DOWN、OK 分别通过 0 Ω、1 kΩ 和 2.2 kΩ 把节点连接到地。

| 状态 | 理论电压 | 量产实测识别窗口 |
| --- | ---: | ---: |
| UP | 0 mV | `[0, 150)` mV |
| DOWN | 300 mV | `[150, 447)` mV |
| OK | 595 mV | `[447, 1900)` mV |
| 松开 | 3300 mV | 不属于任何窗口 |

内部上拉不得替代外部 10 kΩ 电阻。修改电阻梯、ADC attenuation 或 PCB 后，必须在目标电池与温度范围内重新测量松开、UP、DOWN、OK，再修改 `BSP_BTN_MV_TABLE`。

任意功能键都是低电平唤醒源。`bsp_button_prepare_wakeup()` 停止按键 timer，并把 GPIO0 从 ADC 切换为无内部上下拉的数字输入。Light sleep 使用 GPIO 低电平加 2 秒 timer；deep sleep 使用 GPIO 低电平加 5 秒 timer。`CONFIG_ESP_SLEEP_GPIO_ENABLE_INTERNAL_RESISTORS=n` 防止休眠内部上拉与量产电阻并联。Light sleep 唤醒后，`bsp_button_resume_after_wakeup()` 恢复 ADC1_CH0 和按键轮询。Deep sleep 会重启应用。

## 共享 I2C

I2C0 使用 SDA GPIO10 和 SCL GPIO7。两条线分别通过 2.2 kΩ 外部电阻上拉到 3.3 V；固件同时启用内部上拉。ES8311 使用 7-bit 地址 `0x18`；CW2017 使用 `0x63` 和 100 kHz。量产板扫描必须同时看到两个器件。

只有 `bsp_i2c` 创建总线。扫描在现有 handle 上使用 `i2c_master_probe()`，范围为 `0x08` 到 `0x77`。扫描完成但设备数为零表示诊断失败，不表示设备发现成功。只有 codec control adapter 因 API 要求 8-bit 地址而接收 `0x18 << 1`；ESP-IDF I2C API 继续使用 7-bit 地址。

## ES8311 音频

MCU 是 I2S master。I2S0 TX/RX 共用 MCLK GPIO6、BCLK GPIO5 和 WS GPIO3；播放使用 DOUT GPIO2，录音使用 DIN GPIO4。Demo 在物理双 slot 标准 I2S 总线上以 16 kHz、16-bit、mono PCM 打开。

- PCM I/O 前调用 `bsp_audio_set_format()`。格式变化必须 close 并重新 open `esp_codec_dev`；删除该流程会保留旧采样时钟。
- 保持 `no_dac_ref=true`；设为 false 会把 mono 输入接到 DAC reference，导致录音为零。
- 麦克风模拟增益为 30 dB；输出音量是独立的 0–100 值。
- 功放由电池供电且没有使能 GPIO。固定 `pa_voltage=4.2` 用于满电增益补偿，不表示稳压电源轨。
- PCM 读写会阻塞，必须在工作任务运行。I2S DMA 使用六个 descriptor，每个 240 frame。
- 三秒录音 buffer 在 16 kHz、16-bit、mono 下准确为 96,000 bytes。更长录音使用分块处理或存储，禁止假设存在 PSRAM。

## CW2017 电量计

每台量产设备包含 520 mAh 电芯和地址 `0x63` 的 CW2017。初始化读取 VERSION、把 CONFIG 写为 `0x00`、等待 100 ms，并使用芯片内置 Li-Poly profile。仓库不写自定义电芯 profile。

SOC 来自 `0x04–0x05`；大于 100 时返回 `-1`。电压来自 `0x02–0x03` 的 14-bit 值，使用 `raw × 312.5 µV` 换算。事务在 100 kHz 下使用 100 ms timeout。`ESP_ERR_NOT_FOUND` 表示板卡、供电、I2C 或焊接故障，并且只禁用 Battery 页面。报告的 SOC 是电量计读数，不是完整充放电标定容量结果。

## Flash、控制台与内存

构建使用 8 MB Flash、USB Serial/JTAG 且无 PSRAM。分区表固定如下：

| 分区 | 偏移 | 大小 |
| --- | ---: | ---: |
| NVS | `0x9000` | `0x6000` |
| PHY data | `0xF000` | `0x1000` |
| Factory application | `0x10000` | `0x300000` |

没有 OTA slot。合并交付镜像从 `0x0` 开始。禁止把控制台切换到默认 UART0，因为 TX GPIO21 与背光冲突。

增加 LVGL buffer、音频 buffer、任务栈或无线分配时，必须报告构建内存输出，并测量运行时最小空闲堆与最大连续块。总空闲堆不能证明连续分配一定成功。

## 扩展规则

- 可复用硬件能力必须增加 `bsp_<feature>.h`、BSP 实现、`bsp_pins.h` 常量、CMake 依赖、阻塞/所有权说明和实机验收。
- 页面必须实现 `enter`、`exit` 和 `key`，在 `main/demo.h` 声明、加入 `main/CMakeLists.txt`，并在 `DEMOS[]` 中注册及定义初始化失败状态。
- 应用代码不得复制引脚、地址、面板尺寸或电阻计算窗口。
- 可脱离硬件运行的状态机、协议、计时、持久化 codec 和布局计算必须与 ESP-IDF/LVGL 分离，并由主机测试覆盖。

## 构建与实机验收

使用 ESP-IDF 5.5.3 和 `./tools/validate.sh` 构建。把 `build/FoloToy-AI-Passport-full.bin` 从 `0x0` 烧录。构建结果永远不等于实机结果。

| 子系统 | 必须观察的实机结果 |
| --- | --- |
| 启动/I2C | USB 日志稳定；无重启循环/assert/watchdog；扫描显示 `0x18` 和 `0x63` |
| 显示 | 方向、裁剪、反色、RGB565 字节序正确；0/25/50/75/100% 背光正常 |
| 按键 | 松开及 UP/DOWN/OK mV；PRESS/CLICK/DOUBLE/LONG；菜单循环和 OK 长按返回 |
| 音频 | 1 kHz 音调速度正确；录音非零；回放速度、格式切换和页面退出正常 |
| 电池 | 地址 `0x63`；SOC 位于 `0–100`；显示电压；I2C 错误被隔离 |
| Wi-Fi | 显示 SSID/RSSI；可重新扫描；连续进出十次无失败 |
| Bluetooth LE | 手机看到 `FoloPassport`；可重启广播；退出后广播停止；连续进出十次正常 |
| Light sleep | UP、DOWN、OK 分别原地唤醒；timer 在 2 秒后唤醒；背光和 ADC 按键恢复 |
| Deep sleep | UP、DOWN、OK 分别重启应用；timer 在 5 秒后重启；UI 显示原因与保留计数 |
| 内存/生命周期 | 记录最小空闲堆和最大连续块；页面循环十次不遗留任务、timer 或对象 |

## 排障

| 症状 | 检查项 |
| --- | --- |
| 背光亮但无图 | CS/DC/MOSI/SCLK、软件复位、量产面板序列、display-on 命令 |
| 颜色或方向错误 | `swap_bytes`、RGB 顺序、反色、LVGL rotation 覆盖 panel mirror |
| 按键误判 | 10 kΩ 上拉、实测 mV、识别窗口、`ADC_ATTEN_DB_12` |
| `adc1 is already in use` | 按键 BSP 之外创建了第二个 ADC1 oneshot unit |
| 两个 I2C 器件同时消失 | 创建了第二条 I2C0 总线，或缺少 2.2 kΩ 上拉/3.3 V |
| 音频速度错误或录音为零 | 格式 close/open、256×fs MCLK、`no_dac_ref`、DIN GPIO4、30 dB 输入增益 |
| Battery 页面失败 | CW2017 `0x63`、3.3 V I2C 上拉、供电、焊接、SOC 启动延时 |
| Light sleep 立即唤醒 | 入睡前功能键仍为低电平，或 GPIO0 缺少 10 kΩ 上拉 |
| Light sleep 后按键失效 | GPIO 唤醒后 ADC 恢复或按键 timer resume 失败 |
| Deep sleep 未重启 | GPIO0/timer source、启动 wake cause、10 kΩ 上拉、RTC 保留计数 |
| Wi-Fi/BLE 第二次进入失败 | 页面退出时没有停止并反初始化当前 stack |
| CJK 文本显示方框 | 没有加入 CJK 子集字体或混排 fallback |

最终交付分别报告 `Build`、`Host tests`、`Device tests` 和 `Unverified`。
