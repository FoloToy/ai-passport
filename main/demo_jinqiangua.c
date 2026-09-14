// main/demo_jinqiangua.c
// King Wen Jin Qian Gua (文王金钱卦) application for FoloToy AI Passport.
// Follows ui_pixel theme, 240x320 resolution, 3-button navigation, and battery SOC rules.
#include "demo.h"
#include "demo_jinqiangua.h"
#include "jinqiangua_logic.h"
#include "bsp_display.h"
#include "bsp_battery.h"
#include "bsp_audio.h"
#include "ui_pixel.h"
#include "lvgl.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "demo_jinqiangua";

#define YAO_WIDTH   64
#define YAO_HEIGHT  7
#define YAO_GAP     8

static lv_obj_t *s_scr;
static lv_obj_t *s_panel;
static lv_obj_t *s_battery_lbl;
static lv_obj_t *s_mascot;
static lv_obj_t *s_title_lbl;
static lv_obj_t *s_subtitle_lbl;
static lv_obj_t *s_info_lbl;
static lv_obj_t *s_fortune_lbl;
static lv_obj_t *s_advice_lbl;
static lv_obj_t *s_prompt_lbl;
static lv_obj_t *s_yao_objs[6][2]; // [yao_idx][0=left/full, 1=right]
static lv_obj_t *s_coin_objs[3];   // 3 coin visual representations

static lv_timer_t *s_timer;
static jinqiangua_session_t s_session;
static uint32_t s_anim_tick;

static void update_battery(void)
{
    if (!s_battery_lbl) return;
    int soc = bsp_battery_soc();
    if (soc < 0) {
        lv_label_set_text(s_battery_lbl, "--%");
    } else {
        lv_label_set_text_fmt(s_battery_lbl, "%d%%", soc);
    }
}

// Draw a single Yao line (Yang: full bar; Yin: broken bar with middle gap)
static void draw_yao(int index, bool is_yang, bool visible)
{
    // index 0 is Yao 1 (bottom), index 5 is Yao 6 (top)
    // Vertical placement inside s_panel: y = 55 + (5 - index) * 11
    int y = 55 + (5 - index) * 11;
    int center_x = 216 / 2;

    if (!visible) {
        if (s_yao_objs[index][0]) lv_obj_add_flag(s_yao_objs[index][0], LV_OBJ_FLAG_HIDDEN);
        if (s_yao_objs[index][1]) lv_obj_add_flag(s_yao_objs[index][1], LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (is_yang) {
        // Solid continuous line
        if (!s_yao_objs[index][0]) {
            s_yao_objs[index][0] = lv_obj_create(s_panel);
            lv_obj_remove_flag(s_yao_objs[index][0], LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_radius(s_yao_objs[index][0], 0, 0);
            lv_obj_set_style_border_width(s_yao_objs[index][0], 0, 0);
            lv_obj_set_style_pad_all(s_yao_objs[index][0], 0, 0);
            lv_obj_set_style_bg_color(s_yao_objs[index][0], lv_color_hex(UI_INK), 0);
        }
        lv_obj_set_pos(s_yao_objs[index][0], center_x - YAO_WIDTH / 2, y);
        lv_obj_set_size(s_yao_objs[index][0], YAO_WIDTH, YAO_HEIGHT);
        lv_obj_remove_flag(s_yao_objs[index][0], LV_OBJ_FLAG_HIDDEN);

        if (s_yao_objs[index][1]) {
            lv_obj_add_flag(s_yao_objs[index][1], LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        // Broken line: left segment and right segment
        int seg_w = (YAO_WIDTH - YAO_GAP) / 2; // 28px

        // Left segment
        if (!s_yao_objs[index][0]) {
            s_yao_objs[index][0] = lv_obj_create(s_panel);
            lv_obj_remove_flag(s_yao_objs[index][0], LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_radius(s_yao_objs[index][0], 0, 0);
            lv_obj_set_style_border_width(s_yao_objs[index][0], 0, 0);
            lv_obj_set_style_pad_all(s_yao_objs[index][0], 0, 0);
            lv_obj_set_style_bg_color(s_yao_objs[index][0], lv_color_hex(UI_INK), 0);
        }
        lv_obj_set_pos(s_yao_objs[index][0], center_x - YAO_WIDTH / 2, y);
        lv_obj_set_size(s_yao_objs[index][0], seg_w, YAO_HEIGHT);
        lv_obj_remove_flag(s_yao_objs[index][0], LV_OBJ_FLAG_HIDDEN);

        // Right segment
        if (!s_yao_objs[index][1]) {
            s_yao_objs[index][1] = lv_obj_create(s_panel);
            lv_obj_remove_flag(s_yao_objs[index][1], LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_radius(s_yao_objs[index][1], 0, 0);
            lv_obj_set_style_border_width(s_yao_objs[index][1], 0, 0);
            lv_obj_set_style_pad_all(s_yao_objs[index][1], 0, 0);
            lv_obj_set_style_bg_color(s_yao_objs[index][1], lv_color_hex(UI_INK), 0);
        }
        lv_obj_set_pos(s_yao_objs[index][1], center_x - YAO_WIDTH / 2 + seg_w + YAO_GAP, y);
        lv_obj_set_size(s_yao_objs[index][1], seg_w, YAO_HEIGHT);
        lv_obj_remove_flag(s_yao_objs[index][1], LV_OBJ_FLAG_HIDDEN);
    }
}

static void draw_coin(int index, bool is_head, bool visible)
{
    if (!visible) {
        if (s_coin_objs[index]) lv_obj_add_flag(s_coin_objs[index], LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (!s_coin_objs[index]) {
        s_coin_objs[index] = lv_obj_create(s_panel);
        lv_obj_remove_flag(s_coin_objs[index], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(s_coin_objs[index], 28, 28);
        lv_obj_set_style_radius(s_coin_objs[index], 14, 0);
        lv_obj_set_style_border_width(s_coin_objs[index], 2, 0);
        lv_obj_set_style_border_color(s_coin_objs[index], lv_color_hex(UI_INK), 0);
        lv_obj_set_style_pad_all(s_coin_objs[index], 0, 0);

        // Center square hole of Chinese coin
        lv_obj_t *hole = lv_obj_create(s_coin_objs[index]);
        lv_obj_remove_flag(hole, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(hole, 8, 8);
        lv_obj_set_style_radius(hole, 0, 0);
        lv_obj_set_style_border_width(hole, 1, 0);
        lv_obj_set_style_border_color(hole, lv_color_hex(UI_INK), 0);
        lv_obj_set_style_bg_color(hole, lv_color_hex(UI_PAPER), 0);
        lv_obj_center(hole);
    }

    // Colors: Head = Golden Yellow, Tail = Orange
    lv_obj_set_style_bg_color(s_coin_objs[index],
        lv_color_hex(is_head ? UI_YELLOW : UI_ORANGE), 0);

    // Position the 3 coins horizontally
    int start_x = 42;
    int x = start_x + index * 46;
    int y = 126;
    lv_obj_set_pos(s_coin_objs[index], x, y);
    lv_obj_remove_flag(s_coin_objs[index], LV_OBJ_FLAG_HIDDEN);
}

static void refresh_ui(void)
{
    if (!s_panel) return;

    if (s_session.state == JINQIANGUA_STATE_IDLE) {
        lv_label_set_text(s_title_lbl, "JIN QIAN GUA");
        lv_label_set_text(s_subtitle_lbl, "King Wen Coin Divination");
        lv_label_set_text(s_info_lbl, "Focus mind on inquiry.\nPress [OK] to toss coins.");
        lv_label_set_text(s_fortune_lbl, "[ 64 HEXAGRAMS ]");
        lv_obj_set_style_text_color(s_fortune_lbl, lv_color_hex(UI_INK), 0);
        lv_label_set_text(s_advice_lbl, "Traditional Zhouyi Oracle.\nEvery shake reveals a Yao.");
        lv_label_set_text(s_prompt_lbl, "[OK] Start  |  [UP/DN] Browse");

        // Hide yaos
        for (int i = 0; i < 6; i++) {
            draw_yao(i, false, false);
        }
        // Show 3 idle coins
        draw_coin(0, true, true);
        draw_coin(1, false, true);
        draw_coin(2, true, true);
    }
    else if (s_session.state == JINQIANGUA_STATE_SHAKING) {
        lv_label_set_text_fmt(s_title_lbl, "SHAKING (%d/6)", s_session.step);
        lv_label_set_text(s_subtitle_lbl, "Tossing 3 Bronze Coins...");

        int heads = s_session.coins[0] + s_session.coins[1] + s_session.coins[2];
        lv_label_set_text_fmt(s_info_lbl, "Step %d: %s (%s)",
                              s_session.step,
                              (heads % 2 == 1) ? "Yang (---)" : "Yin (- -)",
                              (heads % 2 == 1) ? "Solid" : "Broken");

        lv_label_set_text(s_fortune_lbl, "[ DIVINING... ]");
        lv_obj_set_style_text_color(s_fortune_lbl, lv_color_hex(UI_ORANGE), 0);
        lv_label_set_text(s_advice_lbl, "Forming hexagram yaos...\nSincerity guides the oracle.");
        lv_label_set_text(s_prompt_lbl, "Please wait...");

        // Show yaos up to current step
        for (int i = 0; i < 6; i++) {
            if (i < s_session.step) {
                draw_yao(i, s_session.yaos[i] == 1, true);
            } else {
                draw_yao(i, false, false);
            }
        }

        // Show tossed coins
        draw_coin(0, s_session.coins[0] == 1, true);
        draw_coin(1, s_session.coins[1] == 1, true);
        draw_coin(2, s_session.coins[2] == 1, true);
    }
    else if (s_session.state == JINQIANGUA_STATE_COMPLETED || s_session.state == JINQIANGUA_STATE_BROWSE) {
        const jinqiangua_item_t *item = s_session.current_gua;
        if (!item) item = &G_JINQIANGUA_64[0];

        if (s_session.state == JINQIANGUA_STATE_BROWSE) {
            lv_label_set_text_fmt(s_title_lbl, "[%02d/64] %s", item->number, item->name_pinyin);
        } else {
            lv_label_set_text_fmt(s_title_lbl, "#%02d %s", item->number, item->name_pinyin);
        }

        lv_label_set_text_fmt(s_subtitle_lbl, "%s\n%s / %s",
                              item->name_en, item->upper_trigram, item->lower_trigram);

        lv_label_set_text(s_info_lbl, item->poem);
        lv_label_set_text(s_fortune_lbl, item->fortune);

        // Colorize fortune
        if (strstr(item->fortune, "SUPREME") || strstr(item->fortune, "GREAT")) {
            lv_obj_set_style_text_color(s_fortune_lbl, lv_color_hex(0x2E7D32), 0); // Green
        } else if (strstr(item->fortune, "CAUTION") || strstr(item->fortune, "STOP")) {
            lv_obj_set_style_text_color(s_fortune_lbl, lv_color_hex(0xC62828), 0); // Red
        } else {
            lv_obj_set_style_text_color(s_fortune_lbl, lv_color_hex(0xE65100), 0); // Dark Orange
        }

        lv_label_set_text(s_advice_lbl, item->advice);

        if (s_session.state == JINQIANGUA_STATE_BROWSE) {
            lv_label_set_text(s_prompt_lbl, "[UP/DN] Next/Prev | [OK] Toss");
        } else {
            lv_label_set_text(s_prompt_lbl, "[OK] Restart | [UP/DN] Browse");
        }

        // Draw all 6 yaos
        for (int i = 0; i < 6; i++) {
            draw_yao(i, s_session.yaos[i] == 1, true);
        }

        // Hide coins to make room for full explanation
        draw_coin(0, false, false);
        draw_coin(1, false, false);
        draw_coin(2, false, false);
    }
}

static void timer_cb(lv_timer_t *t)
{
    (void)t;
    s_anim_tick++;

    // Update battery SOC every 1s
    if (s_anim_tick % 10 == 0) {
        update_battery();
    }

    // Auto-progress shaking state every 1.2s
    if (s_session.state == JINQIANGUA_STATE_SHAKING) {
        if (s_anim_tick % 12 == 0) {
            uint32_t seed = (uint32_t)rand() ^ (s_anim_tick << 4);
            bool finished = jinqiangua_step_shake(&s_session, seed);
            ui_pixel_mascot_jump(s_mascot);
            refresh_ui();
            if (finished) {
                ESP_LOGI(TAG, "Divination finished: Gua #%d (%s)",
                         s_session.current_gua->number, s_session.current_gua->name_pinyin);
            }
        }
    }
}

void demo_jinqiangua_enter(void)
{
    ESP_LOGI(TAG, "Entering King Wen Jin Qian Gua Demo");
    jinqiangua_init(&s_session);
    s_anim_tick = 0;

    s_scr = ui_pixel_screen_create("JIN QIAN GUA");

    // Battery SOC indicator in top right (x=144, y=16), safely clear of cloud at (x=188, y=8)
    s_battery_lbl = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_battery_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_battery_lbl, lv_color_hex(UI_INK), 0);
    lv_obj_set_pos(s_battery_lbl, 142, 16);
    lv_label_set_text(s_battery_lbl, "--%");
    update_battery();

    // Main parchment panel: 216x232 at (12, 46)
    s_panel = ui_pixel_panel_create(s_scr, 12, 46, 216, 232, UI_PAPER);

    // Title label
    s_title_lbl = lv_label_create(s_panel);
    lv_obj_set_style_text_font(s_title_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_title_lbl, lv_color_hex(UI_INK), 0);
    lv_obj_align(s_title_lbl, LV_ALIGN_TOP_MID, 0, 4);

    // Subtitle label
    s_subtitle_lbl = lv_label_create(s_panel);
    lv_obj_set_style_text_font(s_subtitle_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_subtitle_lbl, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_align(s_subtitle_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_subtitle_lbl, LV_ALIGN_TOP_MID, 0, 22);

    // Fortune badge
    s_fortune_lbl = lv_label_create(s_panel);
    lv_obj_set_style_text_font(s_fortune_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(s_fortune_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_fortune_lbl, LV_ALIGN_TOP_MID, 0, 122);

    // Poem / Summary info
    s_info_lbl = lv_label_create(s_panel);
    lv_obj_set_style_text_font(s_info_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_info_lbl, lv_color_hex(UI_INK), 0);
    lv_obj_set_style_text_align(s_info_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_info_lbl, 200);
    lv_label_set_long_mode(s_info_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_info_lbl, LV_ALIGN_TOP_MID, 0, 138);

    // Advice / Guidance
    s_advice_lbl = lv_label_create(s_panel);
    lv_obj_set_style_text_font(s_advice_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_advice_lbl, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_align(s_advice_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_advice_lbl, 200);
    lv_label_set_long_mode(s_advice_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_advice_lbl, LV_ALIGN_TOP_MID, 0, 158);

    // Bottom prompt
    s_prompt_lbl = lv_label_create(s_panel);
    lv_obj_set_style_text_font(s_prompt_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_prompt_lbl, lv_color_hex(0x666666), 0);
    lv_obj_align(s_prompt_lbl, LV_ALIGN_BOTTOM_MID, 0, -2);

    // Initialize yao object holders
    for (int i = 0; i < 6; i++) {
        s_yao_objs[i][0] = NULL;
        s_yao_objs[i][1] = NULL;
    }
    for (int i = 0; i < 3; i++) {
        s_coin_objs[i] = NULL;
    }

    // Mascot
    s_mascot = ui_pixel_mascot_create(s_scr, 101, 246);

    refresh_ui();

    // 100ms timer
    s_timer = lv_timer_create(timer_cb, 100, NULL);
    lv_screen_load(s_scr);
}

void demo_jinqiangua_exit(void)
{
    ESP_LOGI(TAG, "Exiting King Wen Jin Qian Gua Demo");
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
        s_panel = NULL;
        s_battery_lbl = NULL;
        s_mascot = NULL;
        s_title_lbl = NULL;
        s_subtitle_lbl = NULL;
        s_info_lbl = NULL;
        s_fortune_lbl = NULL;
        s_advice_lbl = NULL;
        s_prompt_lbl = NULL;
        memset(s_yao_objs, 0, sizeof(s_yao_objs));
        memset(s_coin_objs, 0, sizeof(s_coin_objs));
    }
}

void demo_jinqiangua_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    // Global long press on OK returns to main menu, handled in main.c
    if (ev != BSP_BTN_CLICK) return;

    if (!bsp_lvgl_lock(250)) return;

    if (btn == BSP_BTN_OK) {
        if (s_session.state == JINQIANGUA_STATE_IDLE) {
            jinqiangua_start(&s_session);
            ui_pixel_mascot_jump(s_mascot);
            refresh_ui();
        } else if (s_session.state == JINQIANGUA_STATE_SHAKING) {
            // Manual shake step if user clicks OK while shaking
            uint32_t seed = (uint32_t)rand();
            jinqiangua_step_shake(&s_session, seed);
            ui_pixel_mascot_jump(s_mascot);
            refresh_ui();
        } else if (s_session.state == JINQIANGUA_STATE_COMPLETED) {
            // Restart divination
            jinqiangua_reload(&s_session);
            refresh_ui();
        } else if (s_session.state == JINQIANGUA_STATE_BROWSE) {
            // Switch from browse to divination start
            jinqiangua_start(&s_session);
            ui_pixel_mascot_jump(s_mascot);
            refresh_ui();
        }
    } else if (btn == BSP_BTN_UP) {
        if (s_session.state == JINQIANGUA_STATE_IDLE || s_session.state == JINQIANGUA_STATE_COMPLETED) {
            jinqiangua_enter_browse(&s_session);
        } else if (s_session.state == JINQIANGUA_STATE_BROWSE) {
            jinqiangua_browse_prev(&s_session);
        }
        ui_pixel_mascot_jump(s_mascot);
        refresh_ui();
    } else if (btn == BSP_BTN_DOWN) {
        if (s_session.state == JINQIANGUA_STATE_IDLE || s_session.state == JINQIANGUA_STATE_COMPLETED) {
            jinqiangua_enter_browse(&s_session);
        } else if (s_session.state == JINQIANGUA_STATE_BROWSE) {
            jinqiangua_browse_next(&s_session);
        }
        ui_pixel_mascot_jump(s_mascot);
        refresh_ui();
    }

    bsp_lvgl_unlock();
}
