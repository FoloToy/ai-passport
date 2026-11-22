// main/treaai_ui.h — TreaAIPassport 重设计 UI 接口。
// 四个屏幕: Home, Chat, QuickCmd, Settings; 全部用普通 LVGL widget, 不依赖 ui_pixel。
#pragma once

#include "lvgl.h"
#include "treaai.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_SCR_HOME = 0,
    UI_SCR_CHAT,
    UI_SCR_QUICK_CMD,
    UI_SCR_SETTINGS,
} ui_screen_t;

// 初始化 UI (只创建 Home, 不 load); 必须先 bsp_display_init + bsp_lvgl_init 成功
void treaai_ui_init(void);

// 切换屏幕 (内部加 LVGL 锁)
void treaai_ui_show(ui_screen_t scr);

// 按键事件 (从主循环调用, 内部加 LVGL 锁)
void treaai_ui_on_key(bsp_btn_t btn, bsp_btn_ev_t ev);

// AI 回复到达时调用 (worker context, 内部加 LVGL 锁)
void treaai_ui_on_ai_reply(const char *text);

// 状态变化
void treaai_ui_on_state_change(treaai_state_t state);

// 更新电量条 (主循环每秒调一次)
void treaai_ui_update_battery(int soc);

// Home 屏上 AI 状态小标签刷新
void treaai_ui_update_status_label(const char *text);

// 获取当前屏幕 (给应用层)
ui_screen_t treaai_ui_current(void);

#ifdef __cplusplus
}
#endif
