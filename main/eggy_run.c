// eggy_run.c — view/controller for the Eggy Run side-scroller.
#include "eggy_common.h"
#include "eggy_run_model.h"
#include "eggy_sfx.h"
#include "eggy_ui.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#define EGGY_RUN_TICK_MS 30

static eggy_run_model_t s_model;
static lv_obj_t *s_scr;
static lv_obj_t *s_eggy;          // body obj (resizable for duck)
static lv_obj_t *s_obs[EGGY_MAX_OBSTACLES];
static lv_obj_t *s_score_label;
static lv_obj_t *s_battery;
static lv_obj_t *s_status;
static lv_timer_t *s_timer;
static uint64_t s_last_tick_us;
static eggy_run_input_t s_pending = EGGY_RUN_INPUT_NONE;

static uint64_t now_us(void) { return (uint64_t)esp_timer_get_time(); }

static void set_status(const char *t) {
    if (s_status) lv_label_set_text(s_status, t);
}

static void build_obstacle(int i) {
    s_obs[i] = lv_obj_create(s_scr);
    lv_obj_set_size(s_obs[i], EGGY_OBSTACLE_W, EGGY_SPIKE_H);
    lv_obj_set_style_radius(s_obs[i], 2, 0);
    lv_obj_set_style_border_width(s_obs[i], 0, 0);
    lv_obj_set_style_pad_all(s_obs[i], 0, 0);
    lv_obj_clear_flag(s_obs[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_obs[i], LV_OBJ_FLAG_HIDDEN);
}

static void build_scene(void) {
    s_scr = eggy_screen_create();
    eggy_title_create(s_scr, "EGGY RUN");
    s_battery = eggy_battery_badge_create(s_scr);

    s_score_label = eggy_label_create(s_scr, 8, 4, "0",
                                      &lv_font_montserrat_20, EGGY_COLOR_YELLOW);

    // Ground band.
    lv_obj_t *ground = eggy_panel_create(s_scr, 0, EGGY_GROUND_Y,
                                          EGGY_SCREEN_W,
                                          EGGY_SCREEN_H - EGGY_GROUND_Y,
                                          EGGY_COLOR_GROUND);

    (void)ground;
    // Eggy body (resizable). Eyes are children so they move with the body.
    s_eggy = lv_obj_create(s_scr);
    lv_obj_set_style_bg_color(s_eggy, lv_color_hex(EGGY_COLOR_YELLOW), 0);
    lv_obj_set_style_bg_opa(s_eggy, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_eggy, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s_eggy, 0, 0);
    lv_obj_set_style_pad_all(s_eggy, 0, 0);
    lv_obj_clear_flag(s_eggy, LV_OBJ_FLAG_SCROLLABLE);
    for (int e = 0; e < 2; e++) {
        lv_obj_t *eye = lv_obj_create(s_eggy);
        lv_obj_set_size(eye, 4, 4);
        lv_obj_set_style_bg_color(eye, lv_color_hex(0x201830), 0);
        lv_obj_set_style_bg_opa(eye, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(eye, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(eye, 0, 0);
        lv_obj_align(eye, LV_ALIGN_TOP_MID, e == 0 ? -5 : 5, 5);
    }

    for (int i = 0; i < EGGY_MAX_OBSTACLES; i++) {
        build_obstacle(i);
    }

    s_status = lv_label_create(s_scr);
    lv_obj_set_style_text_color(s_status, lv_color_hex(EGGY_COLOR_INK), 0);
    lv_obj_set_style_text_font(s_status, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_status, EGGY_SCREEN_W);
    lv_obj_align(s_status, LV_ALIGN_CENTER, 0, -40);

    eggy_hint_create(s_scr, "UP:JUMP DOWN:DUCK OK:PAUSE");
}

static void draw(void) {
    int h = eggy_run_eggy_h(&s_model);
    lv_obj_set_size(s_eggy, EGGY_W, h);
    lv_obj_set_pos(s_eggy, EGGY_X, (int)s_model.eggy_y - h);

    for (int i = 0; i < EGGY_MAX_OBSTACLES; i++) {
        const eggy_obstacle_t *o = &s_model.obstacles[i];
        if (!o->active) {
            if (!lv_obj_has_flag(s_obs[i], LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_add_flag(s_obs[i], LV_OBJ_FLAG_HIDDEN);
            }
            continue;
        }
        lv_obj_clear_flag(s_obs[i], LV_OBJ_FLAG_HIDDEN);
        int oh, oy;
        if (o->kind == EGGY_OBSTACLE_SPIKE) {
            oy = EGGY_GROUND_Y - EGGY_SPIKE_H;
            oh = EGGY_SPIKE_H;
            lv_obj_set_style_bg_color(s_obs[i], lv_color_hex(EGGY_COLOR_RED), 0);
        } else {
            oy = EGGY_BAR_TOP;
            oh = EGGY_BAR_H;
            lv_obj_set_style_bg_color(s_obs[i], lv_color_hex(EGGY_COLOR_CYAN), 0);
        }
        lv_obj_set_size(s_obs[i], EGGY_OBSTACLE_W, oh);
        lv_obj_set_pos(s_obs[i], o->x, oy);
    }

    lv_label_set_text_fmt(s_score_label, "%d", (int)(s_model.score / 10));
}

static void apply_state_status(void) {
    switch (s_model.state) {
        case EGGY_RUN_READY:     set_status("PRESS OK TO START"); break;
        case EGGY_RUN_PLAYING:   set_status(""); break;
        case EGGY_RUN_PAUSED:    set_status("PAUSED  OK:RESUME"); break;
        case EGGY_RUN_GAME_OVER: set_status("CRASH!  OK:RETRY"); break;
    }
}

static void tick_cb(lv_timer_t *t) {
    (void)t;
    uint64_t now = now_us();
    uint32_t dt = (uint32_t)((now - s_last_tick_us) / 1000);
    s_last_tick_us = now;
    if (dt == 0) dt = 1;
    if (dt > 100) dt = 100;  // clamp after a pause

    eggy_run_input_t in = s_pending;
    s_pending = EGGY_RUN_INPUT_NONE;

    eggy_run_event_t ev = eggy_run_step(&s_model, dt, in);
    if (ev == EGGY_RUN_EVENT_JUMPED) eggy_sfx_play(EGGY_SFX_JUMP);
    else if (ev == EGGY_RUN_EVENT_DUCKED) eggy_sfx_play(EGGY_SFX_DUCK);
    else if (ev == EGGY_RUN_EVENT_CRASHED) eggy_sfx_play(EGGY_SFX_CRASH);

    apply_state_status();
    draw();
}

void eggy_run_enter(void) {
    eggy_rng_seed(&s_model.rng, (uint32_t)now_us());
    eggy_run_reset(&s_model);
    build_scene();
    set_status("PRESS OK TO START");
    draw();
    eggy_sleep_set_in_game(true);
    s_last_tick_us = now_us();
    s_timer = lv_timer_create(tick_cb, EGGY_RUN_TICK_MS, NULL);
    lv_timer_ready(s_timer);
}

void eggy_run_exit(void) {
    if (s_timer) {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }
    if (s_scr) {
        lv_obj_delete(s_scr);
    }
    s_scr = NULL;
    s_eggy = NULL;
    s_status = NULL;
    s_pending = EGGY_RUN_INPUT_NONE;
}

void eggy_run_key(const eggy_input_t *in) {
    eggy_run_input_t req = EGGY_RUN_INPUT_NONE;
    if (in->ev == BSP_BTN_PRESS) {
        if (in->btn == BSP_BTN_UP) req = EGGY_RUN_INPUT_JUMP;
        else if (in->btn == BSP_BTN_DOWN) req = EGGY_RUN_INPUT_DUCK;
    } else if (in->ev == BSP_BTN_CLICK && in->btn == BSP_BTN_OK) {
        if (s_model.state == EGGY_RUN_READY) req = EGGY_RUN_INPUT_START;
        else if (s_model.state == EGGY_RUN_PLAYING) req = EGGY_RUN_INPUT_TOGGLE_PAUSE;
        else if (s_model.state == EGGY_RUN_PAUSED) req = EGGY_RUN_INPUT_TOGGLE_PAUSE;
        else if (s_model.state == EGGY_RUN_GAME_OVER) req = EGGY_RUN_INPUT_RESTART;
    }
    if (req != EGGY_RUN_INPUT_NONE) {
        s_pending = req;  // latest wins; applied on the next tick
    }
}
