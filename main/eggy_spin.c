// eggy_spin.c — view/controller for the Lucky Spin wheel.
#include "eggy_common.h"
#include "eggy_sfx.h"
#include "eggy_spin_model.h"
#include "eggy_ui.h"

#include "esp_timer.h"
#include "lvgl.h"

#include <math.h>

#define EGGY_SPIN_TICK_MS  20

static const char *const EGGY_SPIN_LABELS[EGGY_SPIN_SEGMENTS] = {
    "x2", "x3", "+50", "x5", "x2", "MISS", "x4", "+100",
};
static const uint32_t EGGY_SPIN_COLORS[EGGY_SPIN_SEGMENTS] = {
    EGGY_COLOR_PINK, EGGY_COLOR_CYAN, EGGY_COLOR_YELLOW, EGGY_COLOR_GREEN,
    EGGY_COLOR_PINK, EGGY_COLOR_RED,  EGGY_COLOR_CYAN,   EGGY_COLOR_YELLOW,
};

#define WHEEL_CX  120
#define WHEEL_CY  175
#define WHEEL_R   78

static eggy_spin_model_t s_model;
static lv_obj_t *s_scr;
static lv_obj_t *s_wheel;
static lv_obj_t *s_pointer;
static lv_obj_t *s_result;
static lv_obj_t *s_battery;
static lv_timer_t *s_timer;
static uint64_t s_last_us;

static uint64_t now_us(void) { return (uint64_t)esp_timer_get_time(); }

static void build_wheel(void) {
    s_wheel = lv_obj_create(s_scr);
    lv_obj_set_pos(s_wheel, WHEEL_CX - WHEEL_R, WHEEL_CY - WHEEL_R);
    lv_obj_set_size(s_wheel, WHEEL_R * 2, WHEEL_R * 2);
    lv_obj_set_style_bg_color(s_wheel, lv_color_hex(EGGY_COLOR_PANEL2), 0);
    lv_obj_set_style_bg_opa(s_wheel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_wheel, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(s_wheel, lv_color_hex(EGGY_COLOR_PINK), 0);
    lv_obj_set_style_border_width(s_wheel, 2, 0);
    lv_obj_set_style_pad_all(s_wheel, 0, 0);
    lv_obj_clear_flag(s_wheel, LV_OBJ_FLAG_SCROLLABLE);

    // 8 segment labels arranged in a ring; pointer (rotating) selects one.
    for (int i = 0; i < EGGY_SPIN_SEGMENTS; i++) {
        float a = (i + 0.5f) * (360.0f / EGGY_SPIN_SEGMENTS);
        float rad = a * 3.14159265f / 180.0f;
        int rx = (int)(WHEEL_CX + (WHEEL_R - 16) * sinf(rad) - 14);
        int ry = (int)(WHEEL_CY - (WHEEL_R - 16) * cosf(rad) - 8);
        lv_obj_t *lab = eggy_label_create(s_wheel, rx, ry,
                                           EGGY_SPIN_LABELS[i],
                                           &lv_font_montserrat_14,
                                           EGGY_SPIN_COLORS[i]);
        (void)lab;
    }

    // Pointer: a needle anchored at the wheel center, rotated each tick.
    s_pointer = lv_obj_create(s_scr);
    lv_obj_set_size(s_pointer, 6, 34);
    lv_obj_set_style_bg_color(s_pointer, lv_color_hex(EGGY_COLOR_INK), 0);
    lv_obj_set_style_bg_opa(s_pointer, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_pointer, 3, 0);
    lv_obj_set_style_border_width(s_pointer, 0, 0);
    lv_obj_set_style_pad_all(s_pointer, 0, 0);
    lv_obj_clear_flag(s_pointer, LV_OBJ_FLAG_SCROLLABLE);
    // Base at wheel center: place so bottom-center is at (WHEEL_CX, WHEEL_CY).
    lv_obj_set_pos(s_pointer, WHEEL_CX - 3, WHEEL_CY - 34);
    lv_obj_set_style_transform_pivot_x(s_pointer, 3, 0);
    lv_obj_set_style_transform_pivot_y(s_pointer, 34, 0);
}

static void build_scene(void) {
    s_scr = eggy_screen_create();
    eggy_title_create(s_scr, "LUCKY SPIN");
    s_battery = eggy_battery_badge_create(s_scr);
    build_wheel();
    s_result = lv_label_create(s_scr);
    lv_obj_set_style_text_color(s_result, lv_color_hex(EGGY_COLOR_YELLOW), 0);
    lv_obj_set_style_text_font(s_result, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(s_result, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_result, EGGY_SCREEN_W);
    lv_obj_align(s_result, LV_ALIGN_CENTER, 0, 100);
    lv_label_set_text(s_result, "OK TO SPIN");
    eggy_hint_create(s_scr, "OK:SPIN  HOLD:MENU");
}

static void draw_pointer(void) {
    // LVGL 9: transform_rotation is in 0.1-degree units.
    int angle = (int)(s_model.angle_deg * 10.0f);
    lv_obj_set_style_transform_rotation(s_pointer, angle, 0);
}

static void tick_cb(lv_timer_t *t) {
    (void)t;
    uint64_t now = now_us();
    uint32_t dt = (uint32_t)((now - s_last_us) / 1000);
    s_last_us = now;
    if (dt == 0) dt = 1;
    if (dt > 100) dt = 100;

    bool settled = eggy_spin_step(&s_model, dt);
    draw_pointer();
    if (settled) {
        int seg = s_model.result;
        const char *label = (seg >= 0 && seg < EGGY_SPIN_SEGMENTS)
                            ? EGGY_SPIN_LABELS[seg] : "?";
        lv_label_set_text_fmt(s_result, "%s  OK:AGAIN", label);
        if (label[0] == 'M') {
            eggy_sfx_play(EGGY_SFX_LOSE);
        } else {
            eggy_sfx_play(EGGY_SFX_WIN);
        }
    } else if (s_model.spinning) {
        lv_label_set_text(s_result, "...");
    }
}

void eggy_spin_enter(void) {
    eggy_spin_reset(&s_model, (uint32_t)now_us());
    build_scene();
    draw_pointer();
    eggy_sleep_set_in_game(true);
    s_last_us = now_us();
    s_timer = lv_timer_create(tick_cb, EGGY_SPIN_TICK_MS, NULL);
    lv_timer_ready(s_timer);
}

void eggy_spin_exit(void) {
    if (s_timer) {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }
    if (s_scr) {
        lv_obj_delete(s_scr);
    }
    s_scr = NULL;
    s_wheel = NULL;
    s_pointer = NULL;
}

void eggy_spin_key(const eggy_input_t *in) {
    if (in->ev == BSP_BTN_CLICK && in->btn == BSP_BTN_OK && !s_model.spinning) {
        eggy_sfx_play(EGGY_SFX_START);
        eggy_spin_kick(&s_model);
        lv_label_set_text(s_result, "...");
    }
}
