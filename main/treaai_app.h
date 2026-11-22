// main/treaai_app.h — TreaAIPassport 应用层。
#pragma once

#include "esp_err.h"
#include "treaai.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化整个应用 (BSP + TreaAI + UI + 按键分发 + worker task)
esp_err_t treaai_app_init(void);

// 主循环: 每秒刷新电量等
void treaai_app_tick(void);

// UI 层选中快捷指令后回调这里发送 AI 请求
void treaai_app_on_quick_select(int idx);

// UI 层对话屏长按开始录音 / 松手发送的事件
void treaai_app_on_chat_record_start(void);
void treaai_app_on_chat_record_stop(void);

#ifdef __cplusplus
}
#endif
