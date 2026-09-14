<p align="right">
  <strong>简体中文</strong> · <a href="deep-sleep-peripheral-power-off.md">English</a>
</p>

# 深睡前关闭板载外设

本参考最初沉淀自「音效钥匙扣」v1.4.0 固件的 voice-keychain 版本
（提交 `ce9b13d`）。原真机测试确认固件可在空闲五分钟后休眠、按键唤醒，
且 codec/面板/电量计的深睡功耗下降，但没有测量具体待机电流。

上游 BSP 现在采用了比该历史固件更严格的关闭契约。其自动化契约与 ESP-IDF
编译已检查，实际电流改善幅度仍需在目标板上测量。

## 为什么必须关闭外设

`esp_deep_sleep_start()` 会关闭 MCU 核心，但常通 3.3 V 电源轨上的器件仍然
供电。MCU 入睡前，每个可控外设都必须进入自己的低功耗状态。软件 suspend
可减少工作模块和信号泄漏，但不会切断电源轨。

## 当前终端关闭顺序

配置唤醒源并停止页面持有的异步工作后，在 `esp_deep_sleep_start()` 前按
以下顺序执行：

| 顺序 | 接口 | 结果 |
| ---: | --- | --- |
| 1 | `bsp_battery_sleep()` | 写入 CW2017 CONFIG 睡眠值，等待 5 ms 后回读；写入/回读不符时重试一次。 |
| 2 | `bsp_audio_sleep()` | 直接执行完整 ES8311 suspend 序列，校验六个关键寄存器，失败重试一次，并显式停止两条 I2S channel。 |
| 3 | `bsp_audio_prepare_deep_sleep()` | 将 MCLK、BCLK、WS、DOUT 和 DIN 设为关闭内部上下拉的输入。 |
| 4 | `bsp_i2c_prepare_deep_sleep()` | 两个共享总线外设都完成后，将 SDA/SCL 设为关闭内部上下拉的输入。 |
| 5 | `bsp_display_prepare_deep_sleep()` | 阻止 LVGL flush 后，发送关闭显示和 Sleep In，将背光停在低电平，设置 LCD SPI 安全电平并在深睡期间保持。 |

Wi-Fi 和 Bluetooth LE 协议栈由各自 demo 页持有，进入 Low Power 页前已经
停止。若产品应用让无线服务长期存活，必须在 LCD 步骤前另行停止它们。

外设寄存器失败会记录日志，但不会阻止后续关闭步骤和 deep sleep。I2S/I2C
引脚释放后已进入终端路径；若 deep-sleep 入口意外返回，应重启，不应尝试恢复
已部分脱离的总线。

## 为什么 `esp_codec_dev_close()` 不够

`esp_codec_dev_close()` 只在 codec-dev 输入或输出曾被打开时才调用 ES8311 disable
路径。开机后从未播放或录音时，即使 ES8311 控制接口和 I2S channel 已初始化，
该路径也可能被跳过。

因此 BSP 不再做默认格式的无声 open，而是通过独立控制接口写入 suspend
寄存器。它还将 REG45 设为 `0x01` 以关闭 BCLK/LRCK 内部上拉，回读 `0x00`、
`0x01`、`0x0D`、`0x0E`、`0x12` 和 `0x45`，并在 5 ms 后重试一次完整序列。
无论之前是否打开 PCM，该路径都能执行。

REG0E 按掩码比较，而不是整字节相等。本芯片上该寄存器 bit7 不锁存：写 `0xFF`
读回 `0x7F`，把这次写挪到 REG00 复位脉冲之后也不变。因此只比较序列真正能控制的
bits 0-6。要求整字节 `0xFF` 会让每次休眠都报校验失败，而一条永远失败的校验抓不
出真正的问题——它还会挡住那些用这个返回值决定是否进入 light sleep 的调用方。

## MCU 引脚仍需明确的终端状态

ESP-IDF 在进入 deep sleep 时会隔离未 hold 的数字 GPIO，但显式释放引脚仍有价值：
它可立即停止 I2S 时钟，在最后一笔事务后去掉 MCU 内部 I2C 上拉，并让过渡契约
可自动检查。外部 I2C 上拉仍是硬件负载。

LCD 引脚是不浮空的例外：CS 保持高电平，SCLK、MOSI、DC 和背光保持低电平。
deep-sleep 唤醒后，`bsp_display_init()` 会在 SPI 和 LEDC 接管前解除全局及单引脚 hold。

## 软件无法修复的部分

`BSP_I2S_PA_CTRL = -1` 表示功放使能脚没有接到 MCU。功放待机电流、稳压器静态
电流、外部上拉电流和电池自放电都无法由这些 API 消除。寄存器回读通过后若待机
电流仍偏高，应逐一隔离并测量这些硬件负载。

## 唤醒与恢复边界

上游 Low Power demo 只使用 RTC 定时器唤醒，因为本仓库尚未定义共享 ADC 按键
节点的可靠 deep-sleep 唤醒契约。deep sleep 会重启应用；正常 BSP 初始化将解除 LCD
hold，并重新初始化显示、音频、I2S 和 I2C。light sleep 不得调用任何终端引脚释放
或 LCD hold 接口，而应使用 `bsp_audio_wake()` 恢复音频。
