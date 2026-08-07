// main/demo_battery.c —— CW2017 电量与电压,每秒刷新。
#include "demo.h"
#include "bsp_battery.h"
#include "ui_pixel.h"
#include "lvgl.h"

static lv_obj_t   *s_scr, *s_soc, *s_mv;
static lv_timer_t *s_timer;

// lv_timer 跑在 LVGL 任务里,已持有锁,可直接操作对象。
static void tick(lv_timer_t *t) {
    (void)t;
    int soc = bsp_battery_soc();
    int mv  = bsp_battery_mv();

    if (soc < 0) lv_label_set_text(s_soc, "-- %");
    else         lv_label_set_text_fmt(s_soc, "%d %%", soc);

    if (mv < 0)  lv_label_set_text(s_mv, "-- mV");
    else         lv_label_set_text_fmt(s_mv, "%d mV", mv);

    // 低电量变红,便于一眼判断
    lv_obj_set_style_text_color(s_soc,
        (soc >= 0 && soc < 20) ? lv_color_hex(0xFF5A5A) : lv_color_hex(0x39FF88), 0);
}

void demo_battery_enter(void) {
    s_scr = ui_pixel_screen_create("BATTERY");
    lv_obj_t *panel = ui_pixel_panel_create(s_scr, 24, 67, 192, 157, UI_YELLOW);

    s_soc = lv_label_create(panel);
    lv_obj_set_style_text_font(s_soc, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_soc, lv_color_hex(UI_INK), 0);
    lv_obj_align(s_soc, LV_ALIGN_TOP_MID, 0, 18);
    lv_label_set_text(s_soc, "-- %");

    s_mv = lv_label_create(panel);
    lv_obj_set_style_text_color(s_mv, lv_color_hex(UI_INK), 0);
    lv_obj_align(s_mv, LV_ALIGN_TOP_MID, 0, 52);
    lv_label_set_text(s_mv, "-- mV");

    lv_obj_t *battery = ui_pixel_panel_create(panel, 38, 91, 100, 38, UI_GRASS);
    lv_obj_set_style_border_width(battery, 4, 0);
    ui_pixel_mascot_create(s_scr, 101, 238);

    tick(NULL);                                   // 先立刻显示一次,不用等 1 秒
    s_timer = lv_timer_create(tick, 1000, NULL);
    lv_screen_load(s_scr);
}

void demo_battery_exit(void) {
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_scr) { lv_obj_delete(s_scr); s_scr = NULL; s_soc = s_mv = NULL; }
}

void demo_battery_key(bsp_btn_t btn, bsp_btn_ev_t ev) { (void)btn; (void)ev; }
