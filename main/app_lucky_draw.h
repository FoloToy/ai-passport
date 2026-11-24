// main/app_lucky_draw.h —— 快乐选号应用 UI 接口。
#pragma once

#include "bsp_button.h"

// 创建应用首页(设置页)。必须在 bsp_lvgl_init() 成功后、持有 bsp_lvgl_lock() 时调用。
// 调用后创建常驻 lv_timer(电量刷新),应用生命周期内无需销毁。
void app_lucky_init(void);

// 按键事件入口。在应用事件任务上下文调用;内部自行持锁操作 LVGL。
// 容忍 BSP_BTN_RELEASE 等非映射事件。
void app_lucky_key(bsp_btn_t btn, bsp_btn_ev_t ev);
