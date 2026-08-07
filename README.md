# FoloToy-Card

FoloToy-Card 是一个基于 ESP32-C3 的卡片设备固件示例，使用 ESP-IDF 和 LVGL 构建。项目提供像素风交互界面，并包含显示、按键、音频和电池管理等板级功能的参考实现。

## 功能

- ST7789P3 240 × 320 LCD 显示
- LVGL 像素风菜单与轻量角色动画
- ADC 三按键输入
- ES8311 音频播放与录音
- CW2017 电量和电压读取
- USB Serial/JTAG 日志输出

## 硬件

| 模块 | 型号或说明 |
| --- | --- |
| MCU | ESP32-C3，无 PSRAM |
| 屏幕 | ST7789P3，240 × 320，SPI |
| 音频 | ES8311 Codec |
| 电量计 | CW2017 |
| 按键 | 三按键共用 ADC 输入 |

硬件引脚和面板参数集中定义在 `components/bsp/include/bsp_pins.h`。

## 快速开始

项目使用 ESP-IDF 5.5.3：

```bash
get_idf553
idf.py set-target esp32c3
idf.py build
idf.py flash monitor
```

如未配置 `get_idf553` 快捷命令，也可以通过 ESP-IDF 官方的 `export.sh` 初始化开发环境。

## 操作方式

- `UP` / `DOWN`：移动菜单选项或调整当前功能
- 短按 `OK`：进入菜单或执行操作
- 长按 `OK`：返回主菜单

主菜单包含以下演示页面：

- **Display**：切换测试颜色和背光亮度
- **Button**：显示按键事件及 ADC 电压
- **Audio**：播放测试音或录音回放
- **Battery**：显示当前电量和电池电压

## 项目结构

```text
components/bsp/     板级驱动及公开接口
main/               主程序、LVGL 界面和功能演示
tests/              可在主机运行的轻量逻辑测试
sdkconfig.defaults  ESP32-C3 与 LVGL 默认配置
```

可复用的硬件驱动位于 `components/bsp`，应用界面和演示逻辑位于 `main`。

## 测试

提交修改前至少执行一次完整构建：

```bash
get_idf553
idf.py build
```

显示、音频、电池和实体按键功能需要在 FoloToy-Card 硬件上完成最终验证。
