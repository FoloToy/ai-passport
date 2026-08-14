// main/demo_stopwatch.c —— 秒表:启动/暂停、计圈、清零。
// 时长用 esp_timer_get_time() 以微秒累计(暂停时冻结),LVGL 定时器负责刷新显示。
#include "demo.h"
#include "bsp_button.h"
#include "ui_pixel.h"
#include "stopwatch_core.h"
#include "lvgl.h"
#include "esp_timer.h"
#include <string.h>

static lv_obj_t   *s_scr, *s_time, *s_state, *s_laps;
static lv_timer_t *s_timer;
static lv_obj_t   *s_mascot;

static bool     s_running;
static uint64_t s_elapsed_us;   // 已累计运行时长(暂停时冻结)
static uint64_t s_start_us;     // 本次启动/续跑的时间戳

#define MAX_LAPS 3
static uint64_t s_lap_us[MAX_LAPS];   // [0] 为最新一圈
static int      s_lap_cnt;

static char     s_last_time[16];
static int      s_last_state = -1;

static const char    *STATE_TEXT[]  = { "READY", "RUNNING", "PAUSED" };
static const uint32_t STATE_COLOR[] = { 0x78909C, UI_GRASS_DARK, UI_ORANGE };

static uint64_t now_us(void)
{
    return (uint64_t)esp_timer_get_time();
}

static uint64_t current_us(void)
{
    return s_running ? s_elapsed_us + (now_us() - s_start_us) : s_elapsed_us;
}

static void laps_refresh(void)
{
    char all[MAX_LAPS * 32 + 1] = { 0 };
    for (int i = 0; i < s_lap_cnt; i++) {
        char line[24];
        char tmp[32];
        stopwatch_format(s_lap_us[i], line, sizeof(line));
        snprintf(tmp, sizeof(tmp), "L%d %s", i + 1, line);
        strcat(all, tmp);
        if (i < s_lap_cnt - 1) strcat(all, "\n");
    }
    lv_label_set_text(s_laps, all);
}

static void refresh(void)
{
    char buf[16];
    stopwatch_format(current_us(), buf, sizeof(buf));
    if (strcmp(buf, s_last_time) != 0) {   // 文本没变就跳过,省一次重绘
        strcpy(s_last_time, buf);
        lv_label_set_text(s_time, buf);
    }
    int st = s_running ? 1 : (s_elapsed_us ? 2 : 0);
    if (st != s_last_state) {
        s_last_state = st;
        lv_label_set_text(s_state, STATE_TEXT[st]);
        lv_obj_set_style_text_color(s_state, lv_color_hex(STATE_COLOR[st]), 0);
    }
}

static void tick(lv_timer_t *t)
{
    (void)t;
    refresh();
}

void demo_stopwatch_enter(void)
{
    s_running = false;
    s_elapsed_us = 0;
    s_lap_cnt = 0;
    s_last_time[0] = '\0';
    s_last_state = -1;

    s_scr = ui_pixel_screen_create("STOPWATCH");

    lv_obj_t *time_panel = ui_pixel_panel_create(s_scr, 18, 58, 204, 96, UI_PAPER);

    s_time = lv_label_create(time_panel);
    lv_obj_set_style_text_font(s_time, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_time, lv_color_hex(UI_INK), 0);
    lv_obj_align(s_time, LV_ALIGN_TOP_MID, 0, 6);
    lv_label_set_text(s_time, "00:00.00");

    s_state = lv_label_create(time_panel);
    lv_obj_set_style_text_font(s_state, &lv_font_montserrat_14, 0);
    lv_obj_align(s_state, LV_ALIGN_TOP_MID, 0, 38);
    lv_label_set_text(s_state, "READY");

    lv_obj_t *hint = lv_label_create(time_panel);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x78909C), 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 58);
    lv_label_set_text(hint, "OK:START/STOP\nUP:LAP  DOWN:RESET");

    lv_obj_t *laps_panel = ui_pixel_panel_create(s_scr, 18, 162, 204, 66, UI_PAPER);
    s_laps = lv_label_create(laps_panel);
    lv_obj_set_style_text_font(s_laps, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_laps, lv_color_hex(UI_INK), 0);
    lv_obj_align(s_laps, LV_ALIGN_TOP_LEFT, 9, 6);
    lv_label_set_text(s_laps, "LAPS: press UP while running");

    s_mascot = ui_pixel_mascot_create(s_scr, 101, 238);

    s_timer = lv_timer_create(tick, 20, NULL);
    lv_screen_load(s_scr);
}

void demo_stopwatch_exit(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
        s_time = s_state = s_laps = s_mascot = NULL;
    }
}

void demo_stopwatch_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_CLICK) return;

    if (btn == BSP_BTN_OK) {
        if (s_running) {
            s_elapsed_us += now_us() - s_start_us;   // 冻结当前时长
            s_running = false;
        } else {
            s_start_us = now_us();
            s_running = true;
        }
        ui_pixel_mascot_jump(s_mascot);
    } else if (btn == BSP_BTN_UP) {
        if (s_running) {                             // 运行时记一圈,最新在最上
            for (int i = MAX_LAPS - 1; i > 0; i--)
                s_lap_us[i] = s_lap_us[i - 1];
            s_lap_us[0] = current_us();
            if (s_lap_cnt < MAX_LAPS) s_lap_cnt++;
            laps_refresh();
        }
    } else if (btn == BSP_BTN_DOWN) {
        s_running = false;
        s_elapsed_us = 0;
        s_lap_cnt = 0;
        laps_refresh();
    }
    refresh();
}
