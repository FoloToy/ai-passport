// main/app_lucky_draw.c —— 快乐选号 UI:两页容器 + 三键交互。
// 页面结构为本应用全新设计(非基线 demo 外壳):设置页选个数,结果页看号码球。
// 单 screen 双容器切换,无后台任务;lv_timer 仅刷新电量标签。
//
// 按键映射:
//   UP/DOWN 单击  设置页调整号码个数(1..9,两端夹紧)
//   OK 单击       设置页=开始抽取;结果页=再抽一组
//   OK 长按       结果页返回设置页
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "lucky_draw_logic.h"
#include "lvgl.h"
#include "esp_random.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "lucky_draw";

LV_FONT_DECLARE(lucky_font_16);
LV_FONT_DECLARE(lucky_font_24);
LV_FONT_DECLARE(lucky_font_40);

// 配色:深蓝底、白字,号码球按位置循环六色(与抽取顺序无关,仅装饰)。
#define COLOR_BG      0x14212B
#define COLOR_TEXT    0xF2F5F7
#define COLOR_SUBTLE  0x8FA3B0
#define COLOR_ACCENT  0x2E86DE
#define BALL_COLORS   6
static const uint32_t BALL_PALETTE[BALL_COLORS] = {
    0xE24B4B, 0xEF9F27, 0x97C459, 0x378ADD, 0x7F77DD, 0x1D9E75,
};

static lv_obj_t *s_scr;
static lv_obj_t *s_setup_panel;
static lv_obj_t *s_result_panel;
static lv_obj_t *s_count_label;
static lv_obj_t *s_count_summary;
static lv_obj_t *s_balls[LUCKY_COUNT_MAX];
static lv_obj_t *s_battery_label;
static lv_timer_t *s_battery_timer;
static lucky_state_t s_state;

// 电量:读失败优雅降级;低电量(<20%)标红。lv_timer 跑在 LVGL 任务内,已持锁。
static void battery_tick(lv_timer_t *t) {
    (void)t;
    if (!s_battery_label) return;
    int soc = bsp_battery_soc();
    lv_color_t color = lv_color_hex(COLOR_SUBTLE);
    char text[16];
    if (soc < 0) {
        strcpy(text, "-- %");
    } else {
        snprintf(text, sizeof(text), "%d %%", soc);
        if (soc < 20) color = lv_color_hex(0xFF5A5A);
    }
    lv_label_set_text(s_battery_label, text);
    lv_obj_set_style_text_color(s_battery_label, color, 0);
}

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
    lv_label_set_text(label, "");
    return label;
}

// 号码球:圆形容器,数字居中。重建内容直接改样式和文本,不销毁对象。
static void ball_render(int index, uint8_t value) {
    lv_obj_t *ball = s_balls[index];
    lv_obj_clear_flag(ball, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(ball, lv_color_hex(BALL_PALETTE[index % BALL_COLORS]), LV_PART_MAIN);
    lv_obj_t *num = (lv_obj_t *)lv_obj_get_user_data(ball);
    char text[4];
    snprintf(text, sizeof(text), "%d", value);
    lv_label_set_text(num, text);
}

static void balls_layout(void) {
    // 先隐藏全部球再按 nums_len 显示:否则上一次大个数抽取的球会残留。
    for (int i = 0; i < LUCKY_COUNT_MAX; i++) {
        lv_obj_add_flag(s_balls[i], LV_OBJ_FLAG_HIDDEN);
    }
    int count = s_state.nums_len;
    const int ball_d = 52;
    const int cy0 = 118;                       // 第一行中心
    const int cy_step = 62;

    // 每行最多 3 球:1 球行放中线,2 球行对称,3 球行固定三列。
    int idx = 0;
    int row = 0;
    while (count > 0 && idx < LUCKY_COUNT_MAX) {
        int in_row = count > 3 ? 3 : count;
        for (int c = 0; c < in_row; c++) {
            int cx;
            if (in_row == 1) cx = 120;
            else if (in_row == 2) cx = 91 + c * 58;
            else cx = 62 + c * 58;
            lv_obj_t *ball = s_balls[idx];
            lv_obj_set_pos(ball, cx - ball_d / 2, cy0 + row * cy_step - ball_d / 2);
            ball_render(idx, s_state.nums[idx]);
            idx++;
        }
        count -= in_row;
        row++;
    }
    char text[24];
    snprintf(text, sizeof(text), "本组 %d 个号码", s_state.nums_len);
    lv_label_set_text(s_count_summary, text);
}

static void page_switch(lucky_page_t page) {
    if (page == LUCKY_PAGE_SETUP) {
        lv_obj_add_flag(s_result_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_setup_panel, LV_OBJ_FLAG_HIDDEN);
        char text[8];
        snprintf(text, sizeof(text), "%d", s_state.count);
        lv_label_set_text(s_count_label, text);
    } else {
        lv_obj_add_flag(s_setup_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_result_panel, LV_OBJ_FLAG_HIDDEN);
        balls_layout();
    }
}

static void draw_and_show(void) {
    if (!lucky_draw_numbers(esp_random, s_state.count, s_state.nums)) {
        ESP_LOGE(TAG, "抽取失败: count=%d", s_state.count);
        return;
    }
    s_state.nums_len = s_state.count;
    balls_layout();
}

// 事件派发任务上下文调用;UI 操作自行持锁。
static void handle(lucky_input_t in) {
    bool need_draw = false;
    if (!lucky_handle_input(&s_state, in, &need_draw)) return;
    if (!bsp_lvgl_lock(500)) return;
    if (need_draw) {
        draw_and_show();
        page_switch(LUCKY_PAGE_RESULT);
    } else {
        page_switch(s_state.page);
    }
    bsp_lvgl_unlock();
}

void app_lucky_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    // 容忍 BSP_BTN_RELEASE 等额外事件:不是映射内的组合直接丢弃。
    if (ev == BSP_BTN_PRESS || ev == BSP_BTN_DOUBLE || ev == BSP_BTN_RELEASE) return;
    lucky_input_t in;
    if (ev == BSP_BTN_CLICK) {
        if (btn == BSP_BTN_UP) in = LUCKY_KEY_UP_CLICK;
        else if (btn == BSP_BTN_DOWN) in = LUCKY_KEY_DOWN_CLICK;
        else if (btn == BSP_BTN_OK) in = LUCKY_KEY_OK_CLICK;
        else return;
    } else if (ev == BSP_BTN_LONG && btn == BSP_BTN_OK) {
        in = LUCKY_KEY_OK_LONG;
    } else {
        return;
    }
    handle(in);
}

void app_lucky_init(void) {
    s_state.page = LUCKY_PAGE_SETUP;
    s_state.count = 6;
    s_state.nums_len = 0;

    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(COLOR_BG), LV_PART_MAIN);

    s_battery_label = make_label(s_scr, &lucky_font_16, COLOR_SUBTLE);
    lv_label_set_text(s_battery_label, "-- %");
    lv_obj_align(s_battery_label, LV_ALIGN_TOP_RIGHT, -12, 10);

    // ---- 设置页 ----
    s_setup_panel = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_setup_panel);
    lv_obj_set_size(s_setup_panel, 240, 320);
    lv_obj_clear_flag(s_setup_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = make_label(s_setup_panel, &lucky_font_24, COLOR_TEXT);
    lv_label_set_text(title, "快乐选号");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 46);

    lv_obj_t *hint_pick = make_label(s_setup_panel, &lucky_font_16, COLOR_SUBTLE);
    lv_label_set_text(hint_pick, "选择号码个数");
    lv_obj_align(hint_pick, LV_ALIGN_TOP_MID, 0, 92);

    lv_obj_t *arrow_up = make_label(s_setup_panel, &lucky_font_16, COLOR_ACCENT);
    lv_label_set_text(arrow_up, "▲");
    lv_obj_align(arrow_up, LV_ALIGN_TOP_MID, 0, 126);

    s_count_label = make_label(s_setup_panel, &lucky_font_40, COLOR_TEXT);
    lv_label_set_text(s_count_label, "6");
    lv_obj_align(s_count_label, LV_ALIGN_TOP_MID, 0, 148);

    lv_obj_t *arrow_down = make_label(s_setup_panel, &lucky_font_16, COLOR_ACCENT);
    lv_label_set_text(arrow_down, "▼");
    lv_obj_align(arrow_down, LV_ALIGN_TOP_MID, 0, 206);

    lv_obj_t *hint_keys = make_label(s_setup_panel, &lucky_font_16, COLOR_SUBTLE);
    lv_label_set_text(hint_keys, "上下调整 · 确定开始");
    lv_obj_align(hint_keys, LV_ALIGN_TOP_MID, 0, 278);

    // ---- 结果页 ----
    s_result_panel = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_result_panel);
    lv_obj_set_size(s_result_panel, 240, 320);
    lv_obj_clear_flag(s_result_panel, LV_OBJ_FLAG_SCROLLABLE);

    s_count_summary = make_label(s_result_panel, &lucky_font_16, COLOR_SUBTLE);
    lv_label_set_text(s_count_summary, "本组 0 个号码");
    lv_obj_align(s_count_summary, LV_ALIGN_TOP_MID, 0, 28);

    for (int i = 0; i < LUCKY_COUNT_MAX; i++) {
        lv_obj_t *ball = lv_obj_create(s_result_panel);
        lv_obj_remove_style_all(ball);
        lv_obj_set_size(ball, 52, 52);
        lv_obj_set_style_radius(ball, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ball, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(ball, lv_color_hex(BALL_PALETTE[i % BALL_COLORS]), LV_PART_MAIN);
        lv_obj_set_style_shadow_width(ball, 0, LV_PART_MAIN);
        lv_obj_add_flag(ball, LV_OBJ_FLAG_HIDDEN);
        lv_obj_t *num = make_label(ball, &lucky_font_24, 0xFFFFFF);
        lv_label_set_text(num, "");
        lv_obj_center(num);
        lv_obj_set_user_data(ball, num);
        s_balls[i] = ball;
    }

    lv_obj_t *hint_again = make_label(s_result_panel, &lucky_font_16, COLOR_SUBTLE);
    lv_label_set_text(hint_again, "确定再抽 · 长按返回");
    lv_obj_align(hint_again, LV_ALIGN_TOP_MID, 0, 278);

    lv_obj_add_flag(s_result_panel, LV_OBJ_FLAG_HIDDEN);
    page_switch(LUCKY_PAGE_SETUP);

    battery_tick(NULL);                       // 立即显示一次,不用等刷新周期
    s_battery_timer = lv_timer_create(battery_tick, 30000, NULL);

    lv_screen_load(s_scr);
}
