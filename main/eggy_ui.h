// eggy_ui.h — party-theme LVGL helpers (own screens; no demo/ui_pixel shell).
#pragma once

#include <stdint.h>

#include "lvgl.h"

// Build a fresh party screen and make it active. Caller then adds content.
lv_obj_t *eggy_screen_create(void);

// Rounded panel at (x,y) of (w,h) filled with `color`.
lv_obj_t *eggy_panel_create(lv_obj_t *parent, int x, int y, int w, int h,
                            uint32_t color);

// Label at (x,y). font may be NULL for default.
lv_obj_t *eggy_label_create(lv_obj_t *parent, int x, int y, const char *text,
                            const lv_font_t *font, uint32_t color);

// Big centered title near the top of a screen.
lv_obj_t *eggy_title_create(lv_obj_t *parent, const char *text);

// The round eggy mascot as a movable container: yellow body + two eyes.
// `size` is the body diameter in px; `body_color` overrides the default.
lv_obj_t *eggy_mascot_create(lv_obj_t *parent, int x, int y, int size,
                             uint32_t body_color);

// Battery percentage badge (top-right corner). Refresh with soc read.
lv_obj_t *eggy_battery_badge_create(lv_obj_t *parent);
void eggy_battery_badge_refresh(lv_obj_t *badge);

// Bottom hint line, e.g. "UP/DOWN  OK:START".
lv_obj_t *eggy_hint_create(lv_obj_t *parent, const char *text);
