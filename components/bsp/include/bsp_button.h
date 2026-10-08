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
    BSP_BTN_RELEASE,     // 抬起瞬间。加在末尾以保持上列取值不变,但既有回调现在会
                         // 多收到这一事件:用 switch 分发按键事件的代码需容忍(或忽略)它。
                         // 深睡按键唤醒后,靠它判断"那颗键是否已松手"。
} bsp_btn_ev_t;

// 按键事件回调。运行于 button 组件使用的共享 esp_timer 任务,只能入队或执行同等级
// 的有界操作；勿在其中阻塞、访问 LVGL 或做重活。
typedef void (*bsp_btn_cb_t)(bsp_btn_t btn, bsp_btn_ev_t ev, void *user);

// 成功调用可重复，并更新回调与 user；失败会回滚本次已创建的按键和 ADC 资源。
// ADC 校准失败时返回错误而不是把无效电压解码为按键，修正故障后可重试。
esp_err_t bsp_button_init(bsp_btn_cb_t cb, void *user);

// deep sleep 专用:停掉三个按键设备、释放共享 ADC unit,再把按键脚交回普通数字输入
// 并开启上拉(与板上 10k 外部上拉并联)。level 回填当时该脚电平,1 = 松开。
//
// 为什么必须这样做:ADC 接管后该脚的【数字】电平读回是 0 —— 而 deep sleep 的低电平
// 唤醒比的正是这个值。不恢复数字输入,唤醒条件在入睡瞬间就成立,设备会立刻醒回来。
//
// 调用后按键在本次运行中不再可用,必须立即进入 deep sleep 或重启。(想在休眠前
// 拒绝"按键被按住"的情况,用 bsp_button_read_mv() 与 BSP_BTN_MV_RELEASED_MIN 先判,
// 那时按键还活着。)
esp_err_t bsp_button_prepare_deep_sleep(int *level);

// 读当前 ADC 原始电压(mV)。松开时约 3300;按住某键时约为该键的分压值。
// ★ 换了分压/上拉阻值后,用它测出自己的三档电压,再改 bsp_pins.h 的 BSP_BTN_MV_TABLE。
// 读取失败返回 -1。
int bsp_button_read_mv(void);
