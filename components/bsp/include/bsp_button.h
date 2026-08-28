// components/bsp/include/bsp_button.h
// 三个按键共用一个 ADC 引脚,靠分压电阻区分。电压窗口见 bsp_pins.h。
#pragma once

#include "esp_err.h"

// 按键索引。数量用 bsp_pins.h 的 BSP_BTN_COUNT(硬件属性,归引脚表管),
// 这里不再定义尾项计数,避免出现 BSP_BTN_COUNT / BSP_BTN_COUNT_ 两个近似名字。
typedef enum {
    BSP_BTN_UP = 0,
    BSP_BTN_DOWN,
    BSP_BTN_OK,
} bsp_btn_t;

typedef enum {
    BSP_BTN_PRESS = 0,   // 按下瞬间(低延迟,适合游戏类即时响应)
    BSP_BTN_CLICK,       // 单击(按下并抬起)
    BSP_BTN_DOUBLE,      // 双击
    BSP_BTN_LONG,        // 长按
} bsp_btn_ev_t;

// 按键事件回调。运行于 button 组件的定时器任务,勿在其中阻塞或做重活。
typedef void (*bsp_btn_cb_t)(bsp_btn_t btn, bsp_btn_ev_t ev, void *user);

esp_err_t bsp_button_init(bsp_btn_cb_t cb, void *user);

// 读当前 ADC 原始电压(mV)。松开时约 3300;按住某键时约为该键的分压值。
// BSP_BTN_MV_TABLE 是量产板实测范围;只有新的量产实测结果才能修改该表。
// 读取失败返回 -1。
int bsp_button_read_mv(void);

// 停止 ADC 按键轮询并把 GPIO0 切换为低电平 light/deep sleep 唤醒输入。
// 量产板 10kΩ 外部上拉保持松开态为高电平。
esp_err_t bsp_button_prepare_wakeup(void);

// light sleep 唤醒或入睡失败后恢复 ADC 通道和按键事件轮询。
// deep sleep 成功后系统重启,无需调用。
esp_err_t bsp_button_resume_after_wakeup(void);
