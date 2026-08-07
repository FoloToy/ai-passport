// main/demo_button.c —— 按键事件流 + 实时 ADC 电压。
// 电压显示是本页的核心:换了分压/上拉阻值的开发者靠它重标 BSP_BTN_MV_TABLE。
#include "demo.h"
#include "bsp_button.h"
#include "ui_pixel.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t   *s_scr, *s_mv, *s_log;
static lv_timer_t *s_timer;

#define LOG_LINES 6
static char s_lines[LOG_LINES][32];
static int  s_line_cnt;

static const char *BTN_NAME[] = { "UP", "DOWN", "OK" };
static const char *EV_NAME[]  = { "PRESS", "CLICK", "DOUBLE", "LONG" };

// 每 100ms 刷新一次电压。lv_timer 跑在 LVGL 任务里,已持有锁,可直接操作对象。
static void tick(lv_timer_t *t) {
    (void)t;
    int mv = bsp_button_read_mv();
    if (mv < 0) lv_label_set_text(s_mv, "ADC read failed");
    else        lv_label_set_text_fmt(s_mv, "%d mV", mv);
}

static void log_push(const char *text) {
    if (s_line_cnt < LOG_LINES) {
        snprintf(s_lines[s_line_cnt++], sizeof(s_lines[0]), "%s", text);
    } else {
        for (int i = 0; i < LOG_LINES - 1; i++)
            memcpy(s_lines[i], s_lines[i + 1], sizeof(s_lines[0]));
        snprintf(s_lines[LOG_LINES - 1], sizeof(s_lines[0]), "%s", text);
    }
    char all[LOG_LINES * 32 + 1] = { 0 };
    for (int i = 0; i < s_line_cnt; i++) {
        strcat(all, s_lines[i]);
        if (i < s_line_cnt - 1) strcat(all, "\n");
    }
    lv_label_set_text(s_log, all);
}

void demo_button_enter(void) {
    s_line_cnt = 0;
    s_scr = ui_pixel_screen_create("BUTTON / ADC");
    lv_obj_t *panel = ui_pixel_panel_create(s_scr, 18, 58, 204, 184, UI_PAPER);

    s_mv = lv_label_create(panel);
    lv_obj_set_style_text_font(s_mv, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_mv, lv_color_hex(UI_SKY_DARK), 0);
    lv_obj_align(s_mv, LV_ALIGN_TOP_MID, 0, 8);
    lv_label_set_text(s_mv, "-- mV");

    s_log = lv_label_create(panel);
    lv_obj_set_style_text_color(s_log, lv_color_hex(UI_INK), 0);
    lv_obj_align(s_log, LV_ALIGN_TOP_LEFT, 9, 54);
    lv_label_set_text(s_log, "press any key...");

    ui_pixel_mascot_create(s_scr, 101, 238);

    s_timer = lv_timer_create(tick, 100, NULL);
    lv_screen_load(s_scr);
}

void demo_button_exit(void) {
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_scr) { lv_obj_delete(s_scr); s_scr = NULL; s_mv = s_log = NULL; }
}

void demo_button_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    char line[32];
    snprintf(line, sizeof(line), "%s: %s", BTN_NAME[btn], EV_NAME[ev]);
    log_push(line);
}
