// eggy_tap.c — view/controller for the Quick Tap reaction game.
#include "eggy_common.h"
#include "eggy_sfx.h"
#include "eggy_tap_model.h"
#include "eggy_ui.h"

#include "esp_timer.h"
#include "lvgl.h"

#define EGGY_TAP_TICK_MS  20
#define EGGY_TAP_HOLD_MS  700   // show each round result before the next WAIT

static eggy_tap_model_t s_model;
static lv_obj_t *s_scr;
static lv_obj_t *s_light;       // big center panel
static lv_obj_t *s_light_text;  // text inside the light
static lv_obj_t *s_round;
static lv_obj_t *s_battery;
static lv_obj_t *s_hint;
static lv_timer_t *s_timer;
static uint64_t s_hold_until;
static bool s_pending_ok;

static uint64_t now_us(void) { return (uint64_t)esp_timer_get_time(); }

static void set_light(uint32_t color, const char *text) {
    lv_obj_set_style_bg_color(s_light, lv_color_hex(color), 0);
    lv_label_set_text(s_light_text, text);
}

static void draw_idle(void) {
    if (s_model.phase == EGGY_TAP_WAIT) {
        set_light(EGGY_COLOR_RED, "WAIT...");
    } else if (s_model.phase == EGGY_TAP_GO) {
        set_light(EGGY_COLOR_GREEN, "TAP!");
    } else {
        set_light(EGGY_COLOR_PANEL2, "DONE");
    }
}

static void build_scene(void) {
    s_scr = eggy_screen_create();
    eggy_title_create(s_scr, "QUICK TAP");
    s_battery = eggy_battery_badge_create(s_scr);
    s_round = eggy_label_create(s_scr, 8, 4, "R 1/5",
                                &lv_font_montserrat_14, EGGY_COLOR_DIM);

    s_light = eggy_panel_create(s_scr, 20, 70, 200, 150, EGGY_COLOR_RED);
    s_light_text = lv_label_create(s_light);
    lv_obj_set_style_text_color(s_light_text, lv_color_hex(EGGY_COLOR_INK), 0);
    lv_obj_set_style_text_font(s_light_text, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(s_light_text, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_light_text);

    s_hint = eggy_hint_create(s_scr, "OK:TAP  HOLD:MENU");
}

static void tick_cb(lv_timer_t *t) {
    (void)t;
    uint64_t now = now_us();
    // Hold the previous round's result before arming the next WAIT.
    if (now < s_hold_until) {
        return;
    }
    bool ok = s_pending_ok;
    s_pending_ok = false;

    eggy_tap_result_t r = eggy_tap_event(&s_model, now,
        ok ? EGGY_TAP_INPUT_OK : EGGY_TAP_INPUT_NONE);
    switch (r.kind) {
        case EGGY_TAP_EVENT_GO_NOW:
            eggy_sfx_play(EGGY_SFX_GO);
            break;
        case EGGY_TAP_EVENT_RECORDED:
            eggy_sfx_play(EGGY_SFX_PASSED);
            lv_label_set_text_fmt(s_light_text, "%u ms", (unsigned)r.arg_ms);
            lv_obj_set_style_bg_color(s_light, lv_color_hex(EGGY_COLOR_PANEL2), 0);
            s_hold_until = now + EGGY_TAP_HOLD_MS * 1000;
            break;
        case EGGY_TAP_EVENT_FOULED:
            eggy_sfx_play(EGGY_SFX_CRASH);
            lv_label_set_text(s_light_text, "FOUL!");
            lv_obj_set_style_bg_color(s_light, lv_color_hex(EGGY_COLOR_PANEL2), 0);
            s_hold_until = now + EGGY_TAP_HOLD_MS * 1000;
            break;
        case EGGY_TAP_EVENT_MISSED:
            eggy_sfx_play(EGGY_SFX_LOSE);
            lv_label_set_text(s_light_text, "MISS");
            lv_obj_set_style_bg_color(s_light, lv_color_hex(EGGY_COLOR_PANEL2), 0);
            s_hold_until = now + EGGY_TAP_HOLD_MS * 1000;
            break;
        default:
            break;
    }

    if (s_model.phase == EGGY_TAP_DONE) {
        uint32_t avg = eggy_tap_average_ms(&s_model);
        lv_label_set_text_fmt(s_light_text, "AVG %u ms", (unsigned)avg);
        lv_obj_set_style_bg_color(s_light, lv_color_hex(EGGY_COLOR_PANEL2), 0);
        lv_label_set_text(s_hint, "OK:RETRY  HOLD:MENU");
        s_hold_until = UINT64_MAX;  // stay until OK restarts
        return;
    }

    if (r.kind == EGGY_TAP_EVENT_NONE) {
        draw_idle();
    }
    uint8_t shown = s_model.round + 1;
    if (shown > EGGY_TAP_ROUNDS) shown = EGGY_TAP_ROUNDS;
    lv_label_set_text_fmt(s_round, "R %u/%d", (unsigned)shown, EGGY_TAP_ROUNDS);
}

void eggy_tap_enter(void) {
    eggy_tap_reset(&s_model, (uint32_t)now_us(), now_us());
    build_scene();
    draw_idle();
    eggy_sleep_set_in_game(true);
    s_hold_until = 0;
    s_pending_ok = false;
    s_timer = lv_timer_create(tick_cb, EGGY_TAP_TICK_MS, NULL);
    lv_timer_ready(s_timer);
}

void eggy_tap_exit(void) {
    if (s_timer) {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }
    if (s_scr) {
        lv_obj_delete(s_scr);
    }
    s_scr = NULL;
    s_light = NULL;
    s_pending_ok = false;
}

void eggy_tap_key(const eggy_input_t *in) {
    if (in->ev == BSP_BTN_CLICK && in->btn == BSP_BTN_OK) {
        if (s_model.phase == EGGY_TAP_DONE) {
            // Restart on the next tick.
            eggy_tap_reset(&s_model, (uint32_t)now_us(), now_us());
            s_hold_until = 0;
            lv_label_set_text(s_hint, "OK:TAP  HOLD:MENU");
            draw_idle();
        } else {
            s_pending_ok = true;
        }
    }
}
