// main/demo_display.c —— 色块 + 背光调光。
// 用 LVGL 铺纯色(而非底层 draw_bitmap),这样和菜单共用同一套屏幕管理。
#include "demo.h"
#include "bsp_display.h"
#include "ui_pixel.h"
#include "lvgl.h"

static lv_obj_t *s_scr;
static lv_obj_t *s_swatch;
static lv_obj_t *s_info;
static lv_obj_t *s_mascot;
static int s_color_idx;
static int s_bl_idx;

static const uint32_t COLORS[] = { 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF, 0x000000 };
static const char    *COLOR_NAME[] = { "RED", "GREEN", "BLUE", "WHITE", "BLACK" };
#define COLOR_COUNT (sizeof(COLORS) / sizeof(COLORS[0]))

static const uint8_t BL_LEVELS[] = { 100, 50, 10 };
#define BL_COUNT (sizeof(BL_LEVELS) / sizeof(BL_LEVELS[0]))

static void refresh(void) {
    lv_obj_set_style_bg_color(s_swatch, lv_color_hex(COLORS[s_color_idx]), 0);
    // 文字用与背景相反的明度,保证任何色块上都看得见
    bool dark = (s_color_idx == 2 || s_color_idx == 4);   // BLUE / BLACK
    lv_obj_set_style_text_color(s_info, dark ? lv_color_white() : lv_color_black(), 0);
    lv_label_set_text_fmt(s_info, "%s\n\nBACKLIGHT %d%%\n\nOK: NEXT COLOR\nUP/DOWN: LIGHT",
                          COLOR_NAME[s_color_idx], BL_LEVELS[s_bl_idx]);
}

void demo_display_enter(void) {
    s_color_idx = 0;
    s_bl_idx = 0;
    bsp_display_backlight(BL_LEVELS[s_bl_idx]);

    s_scr = ui_pixel_screen_create("DISPLAY");
    s_swatch = ui_pixel_panel_create(s_scr, 18, 58, 204, 188, COLORS[s_color_idx]);
    s_info = lv_label_create(s_swatch);
    lv_obj_set_style_text_font(s_info, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(s_info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_info);
    s_mascot = ui_pixel_mascot_create(s_scr, 101, 238);
    refresh();
    lv_screen_load(s_scr);
}

void demo_display_exit(void) {
    bsp_display_backlight(100);          // 退出时恢复全亮,免得菜单看不见
    if (s_scr) { lv_obj_delete(s_scr); s_scr = NULL; s_swatch = s_info = s_mascot = NULL; }
}

void demo_display_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    if (ev != BSP_BTN_CLICK) return;
    if (btn == BSP_BTN_OK) {
        s_color_idx = (s_color_idx + 1) % COLOR_COUNT;
        ui_pixel_mascot_jump(s_mascot);
    } else {
        s_bl_idx = (btn == BSP_BTN_UP) ? (s_bl_idx + BL_COUNT - 1) % BL_COUNT
                                       : (s_bl_idx + 1) % BL_COUNT;
        bsp_display_backlight(BL_LEVELS[s_bl_idx]);
    }
    refresh();
}
