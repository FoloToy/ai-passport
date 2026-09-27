// mini_common.h — shared types for the mini-world app.
#pragma once

#include "bsp_button.h"

// A queued button event. The button callback enqueues this; the dispatch task
// drains it under the LVGL lock and routes to mini_world_key.
typedef struct {
    bsp_btn_t btn;
    bsp_btn_ev_t ev;
} mini_input_t;

void mini_world_enter(void);
void mini_world_exit(void);
void mini_world_key(const mini_input_t *in);
