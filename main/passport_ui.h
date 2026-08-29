// main/passport_ui.h —— 六个屏幕的 LVGL 实现与网络事件约定。
//
// 线程约定（强于上游红线）：只有 UI 上下文（LVGL 任务内的定时器回调）
// 允许碰 LVGL 对象。按键回调与网络任务都只往队列投递。
#pragma once

#include "bsp_button.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdbool.h>
#include <stdint.h>

// 网络任务 → UI 上下文的事件
typedef enum {
    PASSPORT_UI_EVT_SYNC_OK = 1,   // /passport/me + 余额/流水已刷新
    PASSPORT_UI_EVT_SYNC_FAIL,
    PASSPORT_UI_EVT_CHECKIN_OK,    // arg1 = awarded 积分
    PASSPORT_UI_EVT_CHECKIN_DUP,   // 今日已签
    PASSPORT_UI_EVT_CHECKIN_FAIL,
    PASSPORT_UI_EVT_TIME_VALID,    // SNTP 校时完成
    PASSPORT_UI_EVT_HEARTBEAT_OK,  // 在馆心跳成功（余额/在馆标记已刷新）
    PASSPORT_UI_EVT_NET_IDLE,      // 一次突发结束（无论成败）
} passport_ui_evt_type_t;

typedef struct {
    passport_ui_evt_type_t type;
    int32_t arg1;
} passport_ui_evt_t;

typedef enum {
    PASSPORT_SCREEN_MENU = 0,
    PASSPORT_SCREEN_IDENTITY,
    PASSPORT_SCREEN_QR,
    PASSPORT_SCREEN_POINTS,
    PASSPORT_SCREEN_CHECKIN,
    PASSPORT_SCREEN_SETTINGS,
    PASSPORT_SCREEN_COUNT,
} passport_screen_t;

// 按键处理返回的动作
typedef enum {
    PASSPORT_ACTION_NONE = 0,
    PASSPORT_ACTION_ENTER_DEMO,    // 设置页长按 OK 5 秒的隐藏入口
} passport_action_t;

// 初始化 UI 模块（不建屏；LVGL 上下文调用）
void passport_ui_init(QueueHandle_t ui_queue);

// 切换到指定屏幕（LVGL 上下文调用）
void passport_ui_show(passport_screen_t screen);

passport_screen_t passport_ui_current(void);

// 分发按键到当前屏幕（LVGL 上下文调用）
passport_action_t passport_ui_handle_key(bsp_btn_t btn, bsp_btn_ev_t ev);

// 处理网络事件（LVGL 上下文调用）
void passport_ui_on_net_event(const passport_ui_evt_t *evt);

// 秒级 tick：二维码窗口轮换、倒计时条、时钟显示（LVGL 上下文调用）
void passport_ui_tick(void);

// 浅睡前清屏敏感内容（QR 页防残影泄露，实现文档第 10 节）
void passport_ui_clear_sensitive(void);
