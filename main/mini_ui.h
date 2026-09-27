// mini_ui.h — mini-world LVGL helpers (own screens; no demo/ui_pixel shell).
#pragma once

#include <stdint.h>

#include "lvgl.h"

// Mini-world palette (hex).
#define MINI_COLOR_BG      0x101820   // dark frame
#define MINI_COLOR_INK     0xF4F1FF
#define MINI_COLOR_DIM     0x9A8BC0
#define MINI_COLOR_YELLOW  0xFFE45C
#define MINI_COLOR_RED     0xFF4A57
// Tile colors (I4 palette entries).
#define MINI_COLOR_SKY     0x6EC6FF
#define MINI_COLOR_GRASS   0x46E46F
#define MINI_COLOR_DIRT   0x8B5A2B
#define MINI_COLOR_STONE  0x8A8A8A
#define MINI_COLOR_WOOD   0x6B4226
#define MINI_COLOR_PLAYER 0xFFE45C

lv_obj_t *mini_screen_create(void);
lv_obj_t *mini_panel_create(lv_obj_t *parent, int x, int y, int w, int h,
                            uint32_t color);
lv_obj_t *mini_label_create(lv_obj_t *parent, int x, int y, const char *text,
                            const lv_font_t *font, uint32_t color);
lv_obj_t *mini_battery_badge_create(lv_obj_t *parent);
void mini_battery_badge_refresh(lv_obj_t *badge);
