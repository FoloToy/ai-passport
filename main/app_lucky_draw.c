// main/app_lucky_draw.c —— 快乐选号 UI:两页容器 + 三键交互。
// 页面结构为本应用全新设计(非基线 demo 外壳):设置页顶部为双玩法标签页
// (当前玩法亮色+底部色条),快乐选号模式下调个数,双色球模式固定 6+1。
// 单 screen 双容器切换,无后台任务;lv_timer 仅刷新电量标签。
//
// 按键映射:
//   UP/DOWN 单击  设置页调整号码个数(1..9,两端夹紧;双色球模式无效)
//   OK 单击       设置页=开始抽取;结果页=再抽一组
//   OK 长按       设置页=切换玩法(标签页高亮随之切换);结果页=返回设置页
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
// 双色球模式:红球统一红、蓝球统一蓝。
#define COLOR_BG      0x14212B
#define COLOR_TEXT    0xF2F5F7
#define COLOR_SUBTLE  0x8FA3B0
#define COLOR_ACCENT  0x2E86DE
#define COLOR_TAB_DIM 0x5A6B7C
#define COLOR_SSQ_RED   0xD8402F
#define COLOR_SSQ_BLUE  0x2E6FDE
#define BALL_COLORS   6
static const uint32_t BALL_PALETTE[BALL_COLORS] = {
    0xE24B4B, 0xEF9F27, 0x97C459, 0x378ADD, 0x7F77DD, 0x1D9E75,
};

static lv_obj_t *s_scr;
static lv_obj_t *s_setup_panel;
static lv_obj_t *s_result_panel;
static lv_obj_t *s_tab_lucky;    // 首页玩法标签:当前玩法亮色+底部色条
static lv_obj_t *s_tab_ssq;
static lv_obj_t *s_bar_lucky;
static lv_obj_t *s_bar_ssq;
static lv_obj_t *s_hint_pick;
static lv_obj_t *s_arrow_up;
static lv_obj_t *s_arrow_down;
static lv_obj_t *s_count_ball;
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
// 双色球模式下:前 6 球统一红、第 7 球蓝,覆盖六色轮换。
static void ball_render(int index, uint8_t value) {
    lv_obj_t *ball = s_balls[index];
    uint32_t color = BALL_PALETTE[index % BALL_COLORS];
    if (s_state.mode == LUCKY_MODE_SSQ) {
        color = (index < LUCKY_SSQ_RED_COUNT) ? COLOR_SSQ_RED : COLOR_SSQ_BLUE;
    }
    lv_obj_clear_flag(ball, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(ball, lv_color_hex(color), LV_PART_MAIN);
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
    if (s_state.mode == LUCKY_MODE_SSQ) {
        snprintf(text, sizeof(text), "红球 %d · 蓝球 %d", LUCKY_SSQ_RED_COUNT, LUCKY_SSQ_BLUE_COUNT);
    } else {
        snprintf(text, sizeof(text), "本组 %d 个号码", s_state.nums_len);
    }
    lv_label_set_text(s_count_summary, text);
}

static void page_switch(lucky_page_t page) {
    if (page == LUCKY_PAGE_SETUP) {
        lv_obj_add_flag(s_result_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_setup_panel, LV_OBJ_FLAG_HIDDEN);
        bool ssq = (s_state.mode == LUCKY_MODE_SSQ);
        // 玩法标签页:当前玩法白字+色条,另一玩法置灰无色条。
        lv_obj_set_style_text_color(s_tab_lucky,
                                    lv_color_hex(ssq ? COLOR_TAB_DIM : COLOR_TEXT), 0);
        lv_obj_set_style_text_color(s_tab_ssq,
                                    lv_color_hex(ssq ? COLOR_TEXT : COLOR_TAB_DIM), 0);
        if (ssq) {
            lv_obj_add_flag(s_bar_lucky, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_bar_ssq, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_bar_ssq, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_bar_lucky, LV_OBJ_FLAG_HIDDEN);
        }
        lv_label_set_text(s_hint_pick, ssq ? "红球 33 选 6 · 蓝球 16 选 1" : "选择号码个数");
        if (ssq) {
            // 双色球个数固定 6+1:隐藏箭头,大彩球红蓝垂直渐变(上红下蓝)。
            lv_obj_add_flag(s_arrow_up, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_arrow_down, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(s_count_ball, lv_color_hex(COLOR_SSQ_RED), LV_PART_MAIN);
            lv_obj_set_style_bg_grad_color(s_count_ball, lv_color_hex(COLOR_SSQ_BLUE), LV_PART_MAIN);
            lv_obj_set_style_bg_grad_dir(s_count_ball, LV_GRAD_DIR_VER, LV_PART_MAIN);
            lv_label_set_text(s_count_label, "6+1");
        } else {
            // 快乐选号:个数球颜色随个数轮换,调整时有色彩反馈。
            lv_obj_clear_flag(s_arrow_up, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_arrow_down, LV_OBJ_FLAG_HIDDEN);
            uint32_t c = BALL_PALETTE[(s_state.count - 1) % BALL_COLORS];
            // 切回快乐选号要关掉双色球留下的渐变。
            lv_obj_set_style_bg_grad_dir(s_count_ball, LV_GRAD_DIR_NONE, LV_PART_MAIN);
            lv_obj_set_style_bg_color(s_count_ball, lv_color_hex(c), LV_PART_MAIN);
            char text[8];
            snprintf(text, sizeof(text), "%d", s_state.count);
            lv_label_set_text(s_count_label, text);
        }
    } else {
        lv_obj_add_flag(s_setup_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_result_panel, LV_OBJ_FLAG_HIDDEN);
        balls_layout();
    }
}

static void draw_and_show(void) {
    if (s_state.mode == LUCKY_MODE_SSQ) {
        if (!lucky_draw_ssq(esp_random, s_state.nums)) {
            ESP_LOGE(TAG, "双色球抽取失败");
            return;
        }
        s_state.nums_len = LUCKY_SSQ_TOTAL;
    } else {
        if (!lucky_draw_numbers(esp_random, s_state.count, s_state.nums)) {
            ESP_LOGE(TAG, "抽取失败: count=%d", s_state.count);
            return;
        }
        s_state.nums_len = s_state.count;
    }
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
    s_state.mode = LUCKY_MODE_LUCKY;
    s_state.count = 6;
    s_state.nums_len = 0;

    s_scr = lv_obj_create(NULL);
    // 深蓝靛垂直渐变,两页共用,比纯色更有层次。
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(0x1B2D4F), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(s_scr, lv_color_hex(0x0E1A30), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(s_scr, LV_GRAD_DIR_VER, LV_PART_MAIN);

    s_battery_label = make_label(s_scr, &lucky_font_16, COLOR_SUBTLE);
    lv_label_set_text(s_battery_label, "-- %");
    lv_obj_align(s_battery_label, LV_ALIGN_TOP_RIGHT, -12, 10);

    // ---- 设置页 ----
    s_setup_panel = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_setup_panel);
    lv_obj_set_size(s_setup_panel, 240, 320);
    lv_obj_clear_flag(s_setup_panel, LV_OBJ_FLAG_SCROLLABLE);

    // 角落装饰球:低透明度大圆,给纯文字页加氛围。
    static const struct { int x, y, r; uint32_t color; } deco[] = {
        {-36, 258, 52, 0x2E86DE},   // 左下,蓝
        {214, 88, 40, 0x7F77DD},    // 右上,紫
    };
    for (int i = 0; i < 2; i++) {
        lv_obj_t *orb = lv_obj_create(s_setup_panel);
        lv_obj_remove_style_all(orb);
        lv_obj_set_size(orb, deco[i].r * 2, deco[i].r * 2);
        lv_obj_set_style_radius(orb, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(orb, LV_OPA_20, LV_PART_MAIN);
        lv_obj_set_style_bg_color(orb, lv_color_hex(deco[i].color), LV_PART_MAIN);
        lv_obj_set_pos(orb, deco[i].x - deco[i].r, deco[i].y - deco[i].r);
    }

    // 标题上方六色圆点条,呼应号码球调色板。
    for (int i = 0; i < BALL_COLORS; i++) {
        lv_obj_t *dot = lv_obj_create(s_setup_panel);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, 10, 10);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(dot, lv_color_hex(BALL_PALETTE[i]), LV_PART_MAIN);
        lv_obj_align(dot, LV_ALIGN_TOP_MID, i * 18 - 50, 24);
    }

    // 玩法标签页:两个玩法名并排,当前玩法亮色+底部色条,一眼可见双玩法。
    s_tab_lucky = make_label(s_setup_panel, &lucky_font_24, COLOR_TEXT);
    lv_label_set_text(s_tab_lucky, "快乐选号");
    lv_obj_align(s_tab_lucky, LV_ALIGN_TOP_MID, -43, 44);

    s_tab_ssq = make_label(s_setup_panel, &lucky_font_24, COLOR_TAB_DIM);
    lv_label_set_text(s_tab_ssq, "双色球");
    lv_obj_align(s_tab_ssq, LV_ALIGN_TOP_MID, 55, 44);

    // 当前玩法底部色条:快乐选号绿、双色球红,与各自大球颜色呼应。
    s_bar_lucky = lv_obj_create(s_setup_panel);
    lv_obj_remove_style_all(s_bar_lucky);
    lv_obj_set_size(s_bar_lucky, 64, 4);
    lv_obj_set_style_radius(s_bar_lucky, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bar_lucky, lv_color_hex(0x1D9E75), LV_PART_MAIN);
    lv_obj_align(s_bar_lucky, LV_ALIGN_TOP_MID, -43, 78);

    s_bar_ssq = lv_obj_create(s_setup_panel);
    lv_obj_remove_style_all(s_bar_ssq);
    lv_obj_set_size(s_bar_ssq, 48, 4);
    lv_obj_set_style_radius(s_bar_ssq, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bar_ssq, lv_color_hex(COLOR_SSQ_RED), LV_PART_MAIN);
    lv_obj_align(s_bar_ssq, LV_ALIGN_TOP_MID, 55, 78);

    s_hint_pick = make_label(s_setup_panel, &lucky_font_16, COLOR_SUBTLE);
    lv_label_set_text(s_hint_pick, "选择号码个数");
    lv_obj_align(s_hint_pick, LV_ALIGN_TOP_MID, 0, 90);

    s_arrow_up = make_label(s_setup_panel, &lucky_font_16, 0xEF9F27);
    lv_label_set_text(s_arrow_up, "▲");
    lv_obj_align(s_arrow_up, LV_ALIGN_TOP_MID, 0, 116);

    // 个数大彩球:颜色随个数轮换,带同色光晕。
    s_count_ball = lv_obj_create(s_setup_panel);
    lv_obj_remove_style_all(s_count_ball);
    lv_obj_set_size(s_count_ball, 88, 88);
    lv_obj_set_style_radius(s_count_ball, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_count_ball, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(s_count_ball, LV_ALIGN_TOP_MID, 0, 142);
    s_count_label = make_label(s_count_ball, &lucky_font_40, 0xFFFFFF);
    lv_label_set_text(s_count_label, "6");
    lv_obj_center(s_count_label);

    s_arrow_down = make_label(s_setup_panel, &lucky_font_16, 0x1D9E75);
    lv_label_set_text(s_arrow_down, "▼");
    lv_obj_align(s_arrow_down, LV_ALIGN_TOP_MID, 0, 236);

    // 底部提示胶囊:半透明蓝底,视觉收尾。
    lv_obj_t *hint_pill = lv_obj_create(s_setup_panel);
    lv_obj_remove_style_all(hint_pill);
    lv_obj_set_size(hint_pill, 196, 34);
    lv_obj_set_style_radius(hint_pill, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(hint_pill, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_bg_color(hint_pill, lv_color_hex(0x2E86DE), LV_PART_MAIN);
    lv_obj_align(hint_pill, LV_ALIGN_TOP_MID, 0, 272);
    lv_obj_t *hint_keys = make_label(hint_pill, &lucky_font_16, COLOR_TEXT);
    lv_label_set_text(hint_keys, "确定开始 · 长按换玩法");
    lv_obj_center(hint_keys);

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
