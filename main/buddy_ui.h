#pragma once

#include <stdint.h>

#include "buddy_types.h"

/* Every entry point requires the caller to hold bsp_lvgl_lock(). */
void buddy_ui_init(void);
void buddy_ui_render(const buddy_ui_snapshot_t *snapshot);
void buddy_ui_show_passkey(uint32_t passkey);
void buddy_ui_tick(uint64_t elapsed_ms);
