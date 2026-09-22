#pragma once

#include "game_model.h"

// All functions must run in the LVGL task or while holding bsp_lvgl_lock().
void game_ui_create(void);
void game_ui_render(const game_model_t *model, int battery_soc);
void game_ui_show_error(const char *message);
