// mini_ui.c — mini-world LVGL helpers.
#include "mini_ui.h"

#include "bsp_battery.h"

static lv_color_t hexc(uint32_t hex) { return lv_color_hex(hex); }

lv_obj_t *mini_screen_create(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, hexc(MINI_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_screen_load(scr);
    return scr;
}

lv_obj_t *mini_panel_create(lv_obj_t *parent, int x, int y, int w, int h,
                            uint32_t color) {
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_set_pos(p, x, y);
    lv_obj_set_size(p, w, h);
    lv_obj_set_style_bg_color(p, hexc(color), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_radius(p, 6, 0);
    lv_obj_set_style_pad_all(p, 0, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}

lv_obj_t *mini_label_create(lv_obj_t *parent, int x, int y, const char *text,
                            const lv_font_t *font, uint32_t color) {
    lv_obj_t *l = lv_label_create(parent);
    if (x >= 0 && y >= 0) lv_obj_set_pos(l, x, y);
    else lv_obj_center(l);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_color(l, hexc(color), 0);
    if (font) lv_obj_set_style_text_font(l, font, 0);
    return l;
}

lv_obj_t *mini_battery_badge_create(lv_obj_t *parent) {
    return mini_label_create(parent, 240 - 50, 4, "--",
                             &lv_font_montserrat_14, MINI_COLOR_DIM);
}

void mini_battery_badge_refresh(lv_obj_t *badge) {
    int soc = bsp_battery_soc();
    if (soc < 0) {
        lv_label_set_text(badge, "--");
        lv_obj_set_style_text_color(badge, hexc(MINI_COLOR_DIM), 0);
        return;
    }
    if (soc > 100) soc = 100;
    lv_label_set_text_fmt(badge, "%d%%", (unsigned)soc);
    lv_obj_set_style_text_color(badge,
        hexc(soc < 20 ? MINI_COLOR_RED : MINI_COLOR_DIM), 0);
}
