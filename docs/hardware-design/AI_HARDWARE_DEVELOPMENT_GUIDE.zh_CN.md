<p align="right">
  <strong>简体中文</strong> · <a href="AI_HARDWARE_DEVELOPMENT_GUIDE.md">English</a>
</p>

# FoloToy AI Passport AI 硬件开发指南

本文是面向 AI 编程助手和新开发者的板级上下文入口。目标不是替代数据手册，而是准确说明**当前仓库已经确认的硬件事实、软件架构、不可随意改变的约束、扩展方式和验收方法**。

> 固件行为以 `components/bsp/include/bsp_pins.h` 和 BSP 实现为准，不得套用通用 ESP32-C3 开发板参数。

文档适用范围：

- 适用对象：本仓库实现的 ESP32-C3 FoloToy AI Passport 板级映射。
- 产品规格见 [specifications.zh_CN.md](specifications.zh_CN.md)；固件行为以 `bsp_pins.h`、BSP 实现、`sdkconfig.defaults`、`partitions.csv` 与 demo 代码为准。
- 代码复核日期：2026-08-26。

## 1. 开始任何任务前

AI 应先完成以下检查：

1. 阅读 `AGENTS.md`、本文件和将要修改的 BSP 头文件/实现；产品背景不清楚时再读 `docs/README.md`，不默认加载全部 README。
2. 执行 `git status --short`，保留用户已有改动，不覆盖、不清理无关文件。
3. 判断修改属于哪一层：可复用硬件能力放入 `components/bsp`；菜单、动画、业务交互和验证页面放入 `main`。
4. 以 `bsp_pins.h` 为当前板卡引脚和面板参数的单一事实来源，不在 `.c` 文件重复写 GPIO、I2C 地址或屏幕尺寸。
5. 硬件相关修改必须位于产品规格和 BSP 明确定义的范围内。

## 2. 硬件总览

当前代码针对 ESP32-C3 FoloToy AI Passport，使用 ESP-IDF 5.5.3。MCU **没有 PSRAM**，外设 DMA 和 UI 都使用内部 RAM。

| 子系统 | 器件/方式 | 总线或资源 | 固件支持 |
| --- | --- | --- | --- |
| MCU | ESP32-C3 | 8 MB Flash、无 PSRAM | 已配置 |
| 显示 | ST7789P3，240 × 320，RGB565 | SPI2，40 MHz，mode 0 | 驱动与验证页 |
| 背光 | LCD LED 背光 | GPIO21，LEDC 5 kHz/10 bit | PWM 亮度控制 |
| 按键 | UP/DOWN/OK 三键电阻分压 | GPIO0 / ADC1_CH0 | 事件与实时电压页 |
| 音频 | ES8311，播放 + 麦克风录音 | I2C 控制 + I2S0 全双工 | 播放与录音页 |
| 电池 | CW2017 电量计 | 共享 I2C0，地址 0x63 | 可缺省 SOC/电压驱动 |
| Wi-Fi | ESP32-C3 2.4 GHz STA | 应用页按需初始化 | 扫描页 |
| Bluetooth LE | ESP32-C3 NimBLE peripheral | 应用页按需初始化 | 不可连接广播页 |
| 低功耗 | ESP32-C3 light/deep sleep | RTC timer 唤醒 | 2 秒 light sleep 和 5 秒 deep sleep 模式 |
| 日志 | USB Serial/JTAG | 原生 USB GPIO18/19 | 已配置 |

## 3. 引脚表与资源所有权

以下是当前 BSP 和构建配置使用的**固件分配表**。数值来自 `bsp_pins.h`；变更硬件映射时只修改该文件，并同步更新本文和实机结果。

| GPIO | 功能 | 方向/外设 | 重要说明 |
| ---: | --- | --- | --- |
| 0 | 三键公共 ADC 节点 | ADC1_CH0 输入 | 外部 10 kΩ 上拉；也是启动相关管脚，硬件改动前需核对 ESP32-C3 strapping 要求 |
| 1 | LCD CS | SPI 输出 | ST7789P3 片选 |
| 2 | I2S DOUT | 输出 | MCU → ES8311，播放数据 |
| 3 | I2S WS | 输出 | MCU 为 I2S master |
| 4 | I2S DIN | 输入 | ES8311 → MCU，录音数据 |
| 5 | I2S BCLK | 输出 | 与收发共用 |
| 6 | I2S MCLK | 输出 | codec 配置要求使用 MCLK |
| 7 | I2C SCL | 双向开漏 | ES8311 与 CW2017 共用 I2C0 |
| 8 | LCD SCLK | SPI 输出 | SPI2，40 MHz，mode 0 |
| 9 | LCD MOSI | SPI 输出 | 当前没有 MISO，不能读屏 |
| 10 | I2C SDA | 双向开漏 | 软件启用内部上拉；实际硬件仍应有合适外部上拉 |
| 18/19 | USB Serial/JTAG | USB | 控制台使用，避免改作普通 GPIO |
| 20 | LCD DC | 输出 | 命令/数据选择 |
| 21 | LCD 背光 PWM | LEDC 输出 | 与常见 UART0 默认 TX 冲突，所以控制台不可切回该默认 TX |

LCD RST 和功放 PA 使能均定义为 `-1`：LCD 复位使用软件路径，功放被视为常通。

当前所有已用 GPIO 为 0–10、18–21。未列出的 GPIO 不属于公开应用接口。

### 3.1 外设所有权与共存关系

| 资源 | 所有者 | 共享规则或冲突 |
| --- | --- | --- |
| SPI2 | 显示 BSP | 当前固件专用于 ST7789P3，未配置 MISO。 |
| LEDC low-speed timer 0/channel 0 | 背光 BSP | 新 PWM 功能必须选择不冲突的 timer/channel，并复核时钟变化。 |
| ADC1 / channel 0 | 按键 BSP | 按键识别与实时电压共用一个 oneshot unit，不可再创建第二个 ADC1 所有者。 |
| I2C0 | `bsp_i2c` | ES8311 与 CW2017 共用唯一 bus handle，客户端不得重建总线。 |
| I2S0 | 音频 BSP | TX/RX 全双工，共用 MCLK/BCLK/WS。 |
| USB Serial/JTAG | 控制台配置 | GPIO18/19 属于当前控制台路径。 |
| 内部 RAM/DMA | 显示、LVGL、音频、无线、任务 | 无 PSRAM；总空闲堆和最大连续块都必须检查。 |
| NVS/网络 event loop | `demo_radio.c` | 为 Wi-Fi/BLE demo 一次性准备；初始化失败时不得擦除无关 NVS 数据。 |
| Wi-Fi/BLE 协议栈 | 各自 demo 页面 | 当前页面进入时启动、退出时释放，不同时常驻。 |

GPIO0 同时是按键 ADC 节点和 ESP32-C3 启动相关管脚；GPIO21 是背光输出，并与常见 UART0 TX 映射冲突。重分配引脚必须复核启动/烧录路径并完成实机验收。

### 3.2 BSP 之外的产品接口

- USB Type-C 2.0 接受 5 V 输入；固件烧录和日志使用 ESP32-C3 USB Serial/JTAG 接口。
- 独立电源键控制硬件电源，与 BSP 暴露的三枚 ADC 功能键相互独立。
- NTAG213 是被动 NFC 标签，不提供 MCU 侧 BSP API。
- LCD 复位使用控制器软件复位路径，功放使能不由 MCU GPIO 控制。

## 4. 软件架构与启动流程

```text
app_main
  ├─ bsp_i2c_init → bsp_i2c_scan
  ├─ bsp_display_init → bsp_lvgl_init → backlight 100%
  ├─ bsp_button_init(on_key)
  ├─ bsp_audio_init
  ├─ bsp_battery_init
  └─ LVGL menu
       ├─ Display demo
       ├─ Button demo
       ├─ Audio demo
       ├─ Battery demo
       ├─ Wi-Fi scan demo
       ├─ Bluetooth LE advertising demo
       └─ Low Power sleep-mode demo
```

显示是 UI 的硬依赖，显示或 LVGL 初始化失败时 `app_main` 直接返回。按键、音频、电池是软依赖：初始化失败的菜单项显示 `[FAIL]`，其他页面仍可用。

公开 BSP API 位于 `components/bsp/include/`：

- `bsp_i2c.h`：共享总线初始化、句柄和扫描。
- `bsp_display.h`：LCD、背光以及可选 LVGL 接入。
- `bsp_button.h`：三键事件与校准电压读取。
- `bsp_audio.h`：codec 初始化、格式、阻塞式 PCM 收发和音量。
- `bsp_battery.h`：SOC 与电压。
- `bsp_pins.h`：硬件常量，不承载业务逻辑。

驱动初始化大多设计为幂等，但当前没有统一 deinit API。不要假设可以在运行时反复销毁和重建总线/驱动。

Wi-Fi、NimBLE 和 light/deep sleep 直接使用 ESP-IDF API，不属于板级 BSP。`demo_radio.c` 只管理 NVS、`esp_netif` 和默认 event loop 这些应用级共享前置。Wi-Fi 和 BLE 页在进入时初始化高内存占用的无线栈，退出时停止并释放；不自动抹除已有 NVS 数据来掩盖分区错误。deep sleep 会按 ESP32-C3 语义重启应用，示例用 RTC slow memory 记录唤醒次数。

## 5. 显示与 LVGL

### 5.1 面板事实

- ST7789P3，物理/逻辑分辨率均为 240 × 320，当前为竖屏。
- SPI2_HOST、MOSI-only、40 MHz、SPI mode 0、8 bit 命令/参数、RGB565。
- LCD 需要反色命令，`BSP_LCD_INVERT_COLOR=1`。若换屏出现负片，只能在实测后调整。
- RST 为 `-1`，`esp_lcd_panel_reset()` 走 SWRESET。
- 当前 gap 为 `(0, 0)`，镜像 X/Y 都关闭。
- 厂商 porch、power、gamma 初始化表在 `bsp_display.c`。它来自特定面板参考例程，**不是通用 ST7789 默认值**；换面板应取得对应供应商序列。
- 不要手写 MADCTL(0x36) 与现有 mirror/rotation 配置竞争。LVGL 注册显示时还会重新设置旋转。

### 5.2 LVGL 内存和线程规则

ESP32-C3 无 PSRAM。当前 LVGL 显示缓冲为 `240 × 20` 像素的单 DMA 缓冲，RGB565 约 9.6 KB；`sdkconfig.defaults` 的 LVGL 内部池为 24 KB。不要直接改为大行数双缓冲，也不要扩大 UI 内存池而不检查内部 RAM、最大连续堆和 I2S DMA 初始化。

LVGL 非线程安全：

- LVGL 定时器回调运行在 LVGL 上下文，可直接操作对象。
- 按键回调运行在 button 组件任务中，必须 `bsp_lvgl_lock()` / `bsp_lvgl_unlock()`。
- 音频任务等其他 FreeRTOS 任务同样必须加锁。
- 获取锁失败时应安全退出，且每条成功加锁路径都必须解锁。
- 页面退出时先停止可能访问页面对象的定时器/任务，再删除 screen，并将静态对象指针置空。

`swap_bytes=true` 是必要配置：LVGL 产生小端 RGB565，而 LCD 的 SPI 数据需要高字节在前。颜色异常时应先核对该标志、RGB/BGR 顺序、反色和面板序列，不要一次修改多个变量。

## 6. ADC 三按键

三个物理按键共享 GPIO0：3.3 V 经外部 10 kΩ 上拉到 ADC 节点，按键按下后分别通过 0 Ω、1 kΩ、2.2 kΩ 接地。

| 状态 | 理论电压 | 当前识别窗口 |
| --- | ---: | ---: |
| UP | 约 0 mV | `[0, 150)` mV（边界语义最终以 button 组件实现为准） |
| DOWN | 约 300 mV | `[150, 447)` mV |
| OK | 约 595 mV | `[447, 1900)` mV |
| 松开 | 约 3300 mV | 不属于任何按键窗口 |

绝不能用约 45 kΩ 且离散性大的 ESP32-C3 内部上拉替代 10 kΩ 外部上拉，否则三个档位会挤在低电压区并受温漂影响。

实现上的关键限制：

- BSP 先创建唯一的 ADC1 oneshot unit，再把同一句柄交给三个 `iot_button` ADC 设备；`bsp_button_read_mv()` 也复用它。不要为电压显示另建 ADC1 unit。
- ADC 衰减为 `ADC_ATTEN_DB_12`，必须与依赖的 button 组件内部配置保持一致。升级组件后要重新核对。
- ADC 校准句柄创建失败不影响按键事件，但 `bsp_button_read_mv()` 返回 `-1`。
- 回调来自 button 组件的定时器任务，不能阻塞、录音、播放或直接做重 UI 操作。
- 事件包括 PRESS、CLICK、DOUBLE、LONG。应用菜单主要消费 CLICK；页面中的 OK LONG 被全局拦截用于返回。

重标阈值时，在 Button 页逐个长按按键记录稳定电压，采集多块板、不同电量和合理温度范围的数据，再把相邻分布之间留裕量设置为边界。不要只用理论分压值。

## 7. 共享 I2C

I2C0 使用 SDA GPIO10、SCL GPIO7。ES8311 地址为 7 bit `0x18`，CW2017 为 7 bit `0x63`。共享总线由 `bsp_i2c.c` 单独拥有，各设备驱动调用幂等的 `bsp_i2c_init()` 并复用句柄。

重要规则：

- 不要在同一个 I2C port 上为扫描或单个设备再创建临时 master bus。
- IDF 5.5.3 中，重复建总线后走错误清理可能解绑正式 SDA/SCL，使两个芯片同时失联。扫描必须用现有总线上的 `i2c_master_probe()`。
- `bsp_i2c_scan()` 扫描 0x08–0x77，适合启动诊断；它返回 OK 只表示扫描完成，不表示一定找到设备。
- CW2017 设备速率明确为 100 kHz。ES8311 控制接口由 `esp_codec_dev` 管理。
- ES8311 创建控制接口时库 API 要求 8 bit 地址，因此传入 `0x18 << 1`；其他使用 7 bit 地址的 ESP-IDF API 不应照搬此移位。

故障定位顺序：确认 `bsp_i2c_init()` 日志 → 扫描是否看到 0x18/0x63 → 检查供电、地、SDA/SCL 和外部上拉 → 检查地址格式 → 检查是否错误创建了第二条同 port 总线。

## 8. ES8311 音频

MCU 是 I2S master，ES8311 是 slave；I2S0 的 TX/RX 全双工通道共享 MCLK/BCLK/WS。当前数据通路为标准 I2S、16 bit slot 设置、双 slot 物理总线，但对外演示以 16 kHz/16 bit/单声道 PCM 打开 codec。

| 信号 | GPIO | 数据方向 |
| --- | ---: | --- |
| MCLK | 6 | MCU → codec |
| BCLK | 5 | MCU → codec |
| WS | 3 | MCU → codec |
| DOUT | 2 | MCU → codec/扬声器播放 |
| DIN | 4 | codec/麦克风 → MCU |

音频约束：

- `bsp_audio_set_format(hz, bits, ch)` 是使用 PCM 前的必要步骤。
- `esp_codec_dev_open()` 对已打开设备会直接返回而不重新配置采样率。因此格式变化时必须 close 后再 open；现有 BSP 已处理，不能删掉。
- close/open 周围的 I2S enable 是为满足驱动内部 disable 状态机，避免 READY 状态报错。
- 不要在 open 后手写 ES8311 REG01–REG06 时钟分频；驱动已根据采样率和 256×fs MCLK 配置。
- `no_dac_ref=true` 对单声道麦克风录音是必要的；改为 false 会让读入通道成为 DAC reference，表现为录音恒零。
- 麦克风模拟输入增益当前为 30 dB；输出音量 API 为 0–100%。增益和音量不是同一个概念。
- `bsp_audio_read/write` 是阻塞调用，不能放在按键回调或 LVGL 任务中。
- I2S DMA 当前为 6 个 descriptor、每个 240 frame。更改 DMA 或 LVGL buffer 前必须联合评估内部 RAM。

Audio demo 使用独立 4 KB 栈任务：OK 播放 1 秒 1 kHz 方波，UP 录 3 秒再回放。录音缓冲约 96 KB，是当前最显著的瞬时堆分配，可能因碎片或其他功能增大而失败。新增长录音应优先采用分块流式处理或外部存储，不可假设存在 PSRAM。

当前 demo 的退出会直接删除音频任务。如果任务正阻塞于 codec 读写，实际硬件上需特别验证退出行为；若扩展为生产逻辑，应设计可取消的分块循环与明确的任务退出握手。

## 9. CW2017 电池计

CW2017 在共享 I2C 地址 0x63。初始化读取 VERSION 确认在线，将 CONFIG 写为 0x00 进入正常模式，等待 100 ms 后使用芯片自带 Li-Poly profile。仓库刻意不写自定义电池 profile，因为开源用户的电池可能不同。

- SOC：读 0x04–0x05，仅返回高字节整数百分比；大于 100 视为未就绪并返回 `-1`。
- 电压：读 0x02–0x03 的 14 bit 值，换算为 `raw × 312.5 µV`，API 返回 mV。
- 事务超时当前为 100 ms，设备时钟为 100 kHz。
- 芯片不应答时初始化返回 `ESP_ERR_NOT_FOUND`，菜单标记失败，但整机继续运行。

SOC 准确度取决于电芯与 profile 的匹配程度。本驱动给出的是电量计读数，不等于实验室标定结果。若产品需要准确 SOC，必须取得电芯参数、CW2017 数据手册和供应商 profile，并完成完整充放电验证。

## 10. Flash、控制台和资源预算

当前产品与固件基线使用 8 MB Flash。`sdkconfig.defaults` 固定使用 8 MB Flash 镜像配置，并关闭 `CONFIG_ESPTOOLPY_HEADER_FLASHSIZE_UPDATE`（不按探测容量回写镜像头，便于 `idf.py merge-bin`）；`partitions.csv` 提供 24 KB NVS、4 KB PHY data、3 MB factory app、位于 `0x356000` 的保护 `cardid`，以及位于 `0x700000` 的永久 Recovery。这不是 ESP-IDF 双槽 OTA 布局；工厂预装 Recovery 负责 BLE 安装，且必须保持固定地址。开机持续按住上键/GPIO0 5 秒时，bootloader 会进入 Recovery。若实机探测结果不是 8 MB，则该设备不符合当前基线；修改项目默认值前应先确认板卡和 Flash 料号。

不得擦除已写身份的设备，也不得移动或覆盖保护分区。社区固件既不包含单机身份，
也不携带替换 Recovery 的数据。详见 [BLE 兼容契约](../development/ble-recovery-compatibility.zh_CN.md)。

控制台固定为 USB Serial/JTAG，不使用 UART0 默认输出，因为其 TX GPIO21 与背光冲突。任何日志接口修改都必须同时检查引脚占用。

内存审查至少关注：

- LVGL 静态内存池 24 KB；
- LCD DMA buffer 约 9.6 KB；
- I2S DMA descriptor/frame buffer；
- Audio demo 96 KB 录音堆；
- Wi-Fi 驱动或 NimBLE host/controller（两个示例不同时常驻）；
- 各 FreeRTOS 任务栈和最大连续空闲块。

新增图片、字体、网络栈、TLS、音频缓存或双缓冲时，应记录 build 后的静态 RAM/Flash 使用，并在运行时记录 free heap 与 largest free block。总 free heap 足够不代表能成功分配大连续缓冲。

## 11. 新功能的正确落点

新增可复用硬件驱动：

1. 在 `components/bsp/include/` 添加 `bsp_<feature>.h`，API 使用 `bsp_` 前缀。
2. 在 `components/bsp/src/` 实现，硬件常量放 `bsp_pins.h`。
3. 更新 `components/bsp/CMakeLists.txt` 的 SRCS/REQUIRES；新第三方组件加入 `idf_component.yml`。
4. 初始化应尽量幂等，错误应返回 `esp_err_t` 并输出包含引脚/地址的诊断日志。
5. 明确 API 的线程、阻塞、内存所有权、任务上下文和失败返回值。

新增硬件验证页：

1. 创建 `main/demo_<feature>.c`，实现 `enter`、`exit`、`key`。
2. 在 `main/demo.h` 声明，在 `main/CMakeLists.txt` 加源文件，在 `main.c` 的 `DEMOS[]` 注册。
3. `enter` 创建并加载自己的 screen；`exit` 先停任务/定时器，再删 screen 和清空指针。
4. 页面文字保持英文；说明性注释可用中文。
5. 慢操作放工作任务，结果通过 LVGL 锁更新界面。
6. 保留 OK 长按返回这一全局交互，不在页面重复实现。

如果菜单项依赖新外设，还需扩展 `s_ok[]` 初始化与失败禁用逻辑。注意当前数组索引与 `DEMOS[]` 顺序隐式对应，修改顺序时必须同步核对。

## 12. 开发环境搭建

全新机器安装、各操作系统依赖以及国际/中国大陆下载线路统一以
[环境引导](../development/environment-setup.zh_CN.md)为准。项目严格使用
**ESP-IDF 5.5.3**。不要直接使用系统中的任意 `idf.py`，也不要将 Arduino、
PlatformIO 或其他 ESP-IDF 版本生成的配置混入当前工程。
编译优先使用 `./tools/validate.sh --firmware` 生成并验证合并固件，烧录优先把该
镜像写入 `0x0`；直接 `idf.py build/flash` 只用于增量开发。

### 12.1 Linux / WSL 准备

以 Ubuntu/Debian 为例，先安装 ESP-IDF 常用依赖：

```bash
sudo apt update
sudo apt install -y git wget flex bison gperf python3 python3-pip \
    python3-venv cmake ninja-build ccache libffi-dev libssl-dev \
    dfu-util libusb-1.0-0
```

WSL 可以用于编译，但烧录和串口监视需要将 USB 设备转发给 WSL；若没有可靠的 USB 转发，可在 WSL 编译、在原生 Linux 或 Windows ESP-IDF 环境烧录。

### 12.2 安装 ESP-IDF 5.5.3

建议把 ESP-IDF 放在仓库之外，避免工具链文件被误提交。以下路径只是示例，可按本机目录调整：

```bash
mkdir -p "$HOME/esp"
cd "$HOME/esp"
git clone --recursive --branch v5.5.3 \
    https://github.com/espressif/esp-idf.git esp-idf-v5.5.3
cd esp-idf-v5.5.3
./install.sh esp32c3
```

如果 clone 时没有完整取得子模块，可在 IDF 目录补执行：

```bash
git submodule update --init --recursive
```

每个新终端都需要激活该环境：

```bash
source "$HOME/esp/esp-idf-v5.5.3/export.sh"
idf.py --version
```

版本输出必须为 ESP-IDF v5.5.3。文档和自动化不得依赖机器专用 alias，AI
也不得擅自修改 shell 启动文件。

### 12.3 获取工程依赖并首次构建

进入项目根目录后执行：

```bash
source <ESP-IDF-v5.5.3-路径>/export.sh
idf.py --version
idf.py set-target esp32c3
idf.py reconfigure
idf.py build
```

首次配置/构建时，ESP-IDF Component Manager 会根据 `components/bsp/idf_component.yml` 获取 LVGL、`esp_lvgl_port`、`button` 和 `esp_codec_dev` 等依赖，并生成 `managed_components/`、`dependencies.lock`、`sdkconfig` 和 `build/` 等状态。不要手工修改 `managed_components` 中的依赖源码；需要改变依赖版本时修改 manifest/lock，并重新构建验证。

`idf.py set-target esp32c3` 会重建目标相关配置。新 checkout、曾为其他芯片配置过的目录或目标变化时必须执行；普通增量构建不必每次执行。

检查生效的关键配置：

```bash
grep -E 'IDF_TARGET|ESP_CONSOLE_USB_SERIAL_JTAG|SPIRAM|FLASHSIZE' sdkconfig
```

预期目标为 ESP32-C3、控制台为 USB Serial/JTAG、Flash 为 8 MB，并且不启用 PSRAM。`sdkconfig.defaults` 只影响新生成配置；已有 `sdkconfig` 不会自动完全跟随 defaults。defaults 变化后应检查配置差异，保留有意设置后运行 `idf.py set-target esp32c3` 重新生成配置。`idf.py fullclean` 只用于清理构建输出。

### 12.4 连接、烧录与监视

使用支持数据传输的 USB 线连接板卡，先识别串口：

```bash
ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
idf.py -p /dev/ttyACM0 flash monitor
```

本板默认使用 ESP32-C3 原生 USB Serial/JTAG，Linux 上通常表现为 `/dev/ttyACM0`，但实际编号可能变化。不要把示例端口硬编码到工程。

退出 monitor 使用 `Ctrl+]`。只监视而不烧录可执行：

```bash
idf.py -p /dev/ttyACM0 monitor
```

若当前用户没有串口权限，在 Ubuntu/Debian 通常可加入 `dialout` 组：

```bash
sudo usermod -aG dialout "$USER"
```

该权限在注销并重新登录后生效。不要用长期 `sudo idf.py` 规避权限问题，否则生成文件可能归 root 所有，后续普通构建会失败。

如果设备不出现，依次检查 USB 线是否支持数据、USB 端口、`dmesg`/设备管理器、板卡供电和下载模式。不要先改 GPIO18/19 或把控制台切到 UART0；UART0 默认 TX GPIO21 会与 LCD 背光冲突。

### 12.5 环境自检与常见问题

```bash
which idf.py
idf.py --version
echo "$IDF_PATH"
python --version
git status --short
idf.py build
```

| 症状 | 处理方式 |
| --- | --- |
| `idf.py: command not found` | 当前终端未 source ESP-IDF 的 `export.sh` |
| IDF 版本不是 5.5.x | 激活 5.5.3 环境；不要继续用错误版本生成配置 |
| Python 包或工具链缺失 | 在对应 IDF 目录重新执行 `./install.sh esp32c3` |
| 组件下载失败 | 检查网络、代理和证书；不要伪造 `managed_components` 内容 |
| 配置与源码不一致 | 先 `idf.py reconfigure`；仍异常时再考虑 `idf.py fullclean` |
| 串口 permission denied | 加入 `dialout` 并重新登录，确认设备节点所属组 |
| 能烧录但无日志 | 确认 USB Serial/JTAG 配置和正确端口，不要默认改用 GPIO21 UART TX |
| 构建目录来自其他 IDF | 激活 5.5.3 后 `idf.py fullclean`，再 set-target/build |

环境验收标准是：`idf.py --version` 正确、`idf.py build` 无错误、设备可烧录、monitor 能看到 `FoloToy AI Passport BSP demo 启动`，并且启动后没有持续重启或 assert。

## 13. 构建与验证

默认优先运行 `./tools/validate.sh --firmware`。以下命令仅用于增量开发：

```bash
source <ESP-IDF-v5.5.3-路径>/export.sh
idf.py --version
idf.py set-target esp32c3   # 新 checkout 或目标变化时
idf.py build
idf.py flash monitor
```

配置陈旧时可执行 `idf.py fullclean`，但这会删除生成的 build 状态；不要用它处理源码工作区问题。

仓库有 `tests/test_ui_pixel_math.c` 轻量逻辑测试源，但当前根 CMake 是 ESP-IDF 工程，未提供统一的 host test 命令。因此 `idf.py build` 是最低自动检查，硬件变更必须上板。

### 通用上板验收

- USB Serial/JTAG 有稳定启动日志，无重启循环、assert、watchdog 和持续错误。
- I2C 扫描看到预期的 0x18；装有 CW2017 的板还应看到 0x63。
- 菜单可用 UP/DOWN 循环导航，OK 单击进入，OK 长按返回。
- 某个可选外设故障只禁用对应页面，不影响其他功能。
- 连续切换页面和反复操作后无堆持续下降、对象悬挂或任务泄漏。

### 按修改类型追加验收

| 修改类型 | 必须观察的实机结果 |
| --- | --- |
| 引脚/I2C | 扫描、所有共享设备、启动冲突、USB 日志 |
| LCD 序列/旋转/颜色 | 红绿蓝白黑色块、方向、边缘裁切、负片、字节序、背光 100/50/10% |
| ADC/按键 | 松开和三键实测 mV、单击/双击/长按、不同电量下的裕量 |
| codec/I2S | 1 kHz 音调频率/速度、录音非零且回放速度正确、格式切换、退出页面 |
| 电池 | 合理 SOC 和 mV、无电量计时正确降级、断续 I2C 的错误恢复表现 |
| Wi-Fi | 扫描总数和 SSID/RSSI 可见、OK 重扫描、反复进出后仍可扫描 |
| Bluetooth LE | 手机看到 `FoloPassport`、OK 重启广播、退出后广播消失、反复进出无重启 |
| light/deep sleep | Low Power 页用 UP/DOWN 选择、OK 执行；light sleep 约 2 秒后原地恢复背光；deep sleep 约 5 秒后重启，页面显示 timer 唤醒和 RTC 保留计数 |
| DMA/内存/UI | build 内存报告、运行时最小堆/最大块、音频与刷屏并发稳定性 |

## 14. 故障症状速查

| 症状 | 优先检查 |
| --- | --- |
| 无画面但背光亮 | LCD CS/DC/MOSI/SCLK、厂商序列、SWRESET、DISPON、SPI mode |
| 颜色颠倒或怪色 | `swap_bytes`、RGB/BGR、反色配置；一次只改一个变量 |
| 画面旋转修改无效 | `bsp_display_lvgl.c` rotation 覆盖底层 mirror |
| 背光或串口异常 | GPIO21 与 UART0 默认 TX 冲突 |
| 三键混淆/误触 | 外部 10 kΩ 上拉、实测电压、阈值、ADC attenuation 一致性 |
| `adc1 is already in use` | 是否又创建了 ADC1 oneshot unit |
| 两个 I2C 芯片同时失联 | 是否在 I2C0 上创建了第二条临时总线 |
| 只找不到 ES8311 | 地址 API 是否要求 `0x18 << 1`、codec 供电 |
| 音频快/慢或变调 | 格式变化是否执行 close/open、采样率/MCLK，勿手改时钟寄存器 |
| 录音全零 | `no_dac_ref` 是否为 true、DIN GPIO4、麦克风通路和输入增益 |
| 录音缓冲分配失败 | C3 无 PSRAM；缩短录音或改流式，检查 largest free block |
| 电量显示 `--` | 0x63 是否应答、SOC 是否读到 >100/0xFF、profile/启动等待 |
| Wi-Fi/BLE 第二次进入失败 | 退出页时是否已停止并 deinit radio stack，NVS/event loop 是否只初始化一次 |
| light sleep 后黑屏 | timer wake source、`esp_light_sleep_start()` 错误日志、唤醒后是否恢复背光 |
| deep sleep 后未重启 | timer wake source、启动日志的 wake cause、页面 RTC 计数；当前 demo 使用 RTC timer 唤醒 |
| 加大 UI 后 I2S NO_MEM | LCD 双缓冲/LVGL pool 与 I2S DMA 争夺内部 RAM |
| 中文显示为方框 | Montserrat 14/20 不含 CJK glyph；编译并选用中文字体子集，为混排配置 fallback，并在真机核对全部字符 |

## 15. AI 提交前自检

- [ ] 变更没有硬编码重复的引脚、地址、尺寸或板级参数。
- [ ] 没有把产品规格或 BSP 未定义的能力写成事实。
- [ ] 没有创建第二个 I2C0 bus 或第二个 ADC1 unit。
- [ ] 非 LVGL 上下文访问 `lv_*` 时持有锁。
- [ ] 阻塞式硬件操作不在按键回调/LVGL 任务中。
- [ ] 页面退出顺序能阻止后台任务访问已删除对象。
- [ ] 音频格式变化仍执行 close/open，ES8311 时钟寄存器和 `no_dac_ref` 未被误改。
- [ ] 内存增加已同时考虑 LVGL、LCD DMA、I2S DMA、任务栈和最大连续堆。
- [ ] `idf.py build` 通过并检查了 warnings；不能构建时说明真实原因。
- [ ] 需要实机验证的项目明确列出，未把“编译通过”写成“硬件验证通过”。
- [ ] `git diff` 只包含任务范围内的改动，用户原有修改保持不动。
