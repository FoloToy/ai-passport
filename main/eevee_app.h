#pragma once

#include "bsp_button.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 启动 Eevee 智能电子工牌应用程序
 */
void eevee_app_start(void);

/**
 * 分发底层硬件按键事件至工牌状态机
 */
void eevee_app_handle_button(bsp_btn_t btn, bsp_btn_ev_t event);

#ifdef __cplusplus
}
#endif
