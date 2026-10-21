#pragma once
#include "esp_err.h"

// ============================================================
// WorkBuddy 集成 Demo
// Enter: 进入 WorkBuddy 状态页
// Exit:  退出并清理
// Key:   UP/DOWN/OK 按键处理
// ============================================================

esp_err_t demo_workbuddy_start(void);
esp_err_t demo_workbuddy_stop(void);
void     demo_workbuddy_enter(void);
void     demo_workbuddy_exit(void);
void     demo_workbuddy_key(bsp_btn_t btn, bsp_btn_ev_t ev);
