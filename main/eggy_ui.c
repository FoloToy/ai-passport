// eggy_ui.c — party-theme LVGL helpers.
#include "eggy_ui.h"

#include "bsp_battery.h"
#include "eggy_common.h"

static lv_color_t hexc(uint32_t hex) {
    return lv_color_hex(hex);
}

lv_obj_t *eggy_screen_create(void) {
    // Create a fresh screen (parent NULL), style it, and make it active.
    // The previous screen is deleted by the calling screen's exit().
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, hexc(EGGY_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_screen_load(scr);
    return scr;
}

lv_obj_t *eggy_panel_create(lv_obj_t *parent, int x, int y, int w, int h,
                            uint32_t color) {
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_set_pos(p, x, y);
    lv_obj_set_size(p, w, h);
    lv_obj_set_style_bg_color(p, hexc(color), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_radius(p, 8, 0);
    lv_obj_set_style_pad_all(p, 0, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}

lv_obj_t *eggy_label_create(lv_obj_t *parent, int x, int y, const char *text,
                            const lv_font_t *font, uint32_t color) {
    lv_obj_t *l = lv_label_create(parent);
    if (x >= 0 && y >= 0) {
        lv_obj_set_pos(l, x, y);
    } else {
        lv_obj_center(l);
    }
    lv_label_set_text(l, text);
    lv_obj_set_style_text_color(l, hexc(color), 0);
    if (font) {
        lv_obj_set_style_text_font(l, font, 0);
    }
    return l;
}

lv_obj_t *eggy_title_create(lv_obj_t *parent, const char *text) {
    lv_obj_t *l = eggy_label_create(parent, 0, 10, text,
                                    &lv_font_montserrat_20, EGGY_COLOR_PINK);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(l, EGGY_SCREEN_W);
    return l;
}

lv_obj_t *eggy_mascot_create(lv_obj_t *parent, int x, int y, int size,
                             uint32_t body_color) {
    if (body_color == 0) {
        body_color = EGGY_COLOR_YELLOW;
    }
    lv_obj_t *m = lv_obj_create(parent);
    lv_obj_set_pos(m, x, y);
    lv_obj_set_size(m, size, size);
    lv_obj_set_style_bg_opa(m, LV_OPA_TRANSP, 0);  // container is invisible
    lv_obj_set_style_border_width(m, 0, 0);
    lv_obj_set_style_pad_all(m, 0, 0);
    lv_obj_clear_flag(m, LV_OBJ_FLAG_SCROLLABLE);

    // Round body fills the container.
    lv_obj_t *body = lv_obj_create(m);
    lv_obj_set_size(body, size, size);
    lv_obj_align(body, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(body, hexc(body_color), 0);
    lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_radius(body, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    // Two dark eyes, placed in the upper area so the eggy "faces" right/forward.
    int eye_r = size > 24 ? 3 : 2;
    int eye_off = size / 5;
    for (int i = 0; i < 2; i++) {
        lv_obj_t *eye = lv_obj_create(m);
        lv_obj_set_size(eye, eye_r * 2, eye_r * 2);
        lv_obj_set_style_bg_color(eye, hexc(0x201830), 0);
        lv_obj_set_style_bg_opa(eye, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(eye, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(eye, 0, 0);
        int dx = (i == 0) ? -eye_off : eye_off;
        lv_obj_align(eye, LV_ALIGN_TOP_MID, dx, size / 4);
    }
    return m;
}

lv_obj_t *eggy_battery_badge_create(lv_obj_t *parent) {
    lv_obj_t *b = eggy_label_create(parent, EGGY_SCREEN_W - 50, 4, "--",
                                    &lv_font_montserrat_14, EGGY_COLOR_DIM);
    return b;
}

void eggy_battery_badge_refresh(lv_obj_t *badge) {
    int soc = bsp_battery_soc();
    if (soc < 0) {
        lv_label_set_text(badge, "--");
        lv_obj_set_style_text_color(badge, hexc(EGGY_COLOR_DIM), 0);
        return;
    }
    if (soc > 100) {
        soc = 100;
    }
    lv_label_set_text_fmt(badge, "%d%%", soc);
    lv_obj_set_style_text_color(badge,
        hexc(soc < 20 ? EGGY_COLOR_RED : EGGY_COLOR_DIM), 0);
}

lv_obj_t *eggy_hint_create(lv_obj_t *parent, const char *text) {
    lv_obj_t *l = eggy_label_create(parent, 0, EGGY_SCREEN_H - 22, text,
                                    &lv_font_montserrat_14, EGGY_COLOR_DIM);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(l, EGGY_SCREEN_W);
    return l;
}
