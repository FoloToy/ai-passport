// eggy_hub.c — the party hub menu screen.
#include "eggy_common.h"
#include "eggy_sfx.h"
#include "eggy_ui.h"

#include "lvgl.h"

static const char *const EGGY_GAME_NAMES[] = { "RUN", "TAP", "SPIN" };
#define EGGY_HUB_COUNT 3

static lv_obj_t *s_scr;
static lv_obj_t *s_cards[EGGY_HUB_COUNT];
static lv_obj_t *s_labels[EGGY_HUB_COUNT];
static lv_obj_t *s_battery;
static int s_sel;

static void refresh_cards(void) {
    for (int i = 0; i < EGGY_HUB_COUNT; i++) {
        bool on = (i == s_sel);
        lv_obj_set_style_bg_color(s_cards[i],
            lv_color_hex(on ? EGGY_COLOR_PANEL2 : EGGY_COLOR_PANEL), 0);
        lv_obj_set_style_border_color(s_cards[i],
            lv_color_hex(on ? EGGY_COLOR_PINK : EGGY_COLOR_PANEL), 0);
        lv_obj_set_style_border_width(s_cards[i], on ? 2 : 0, 0);
        lv_obj_set_style_text_color(s_labels[i],
            lv_color_hex(on ? EGGY_COLOR_INK : EGGY_COLOR_DIM), 0);
    }
}

void eggy_hub_enter(void) {
    s_scr = eggy_screen_create();
    eggy_title_create(s_scr, "EGGY PARTY");
    s_battery = eggy_battery_badge_create(s_scr);

    for (int i = 0; i < EGGY_HUB_COUNT; i++) {
        s_cards[i] = eggy_panel_create(s_scr, 30, 64 + i * 52, 180, 42,
                                        EGGY_COLOR_PANEL);
        s_labels[i] = eggy_label_create(s_cards[i], -1, -1, EGGY_GAME_NAMES[i],
                                        &lv_font_montserrat_20, EGGY_COLOR_INK);
        lv_obj_set_style_text_align(s_labels[i], LV_TEXT_ALIGN_CENTER, 0);
    }
    eggy_mascot_create(s_scr, EGGY_SCREEN_W / 2 - 28, 232, 56, 0);
    eggy_hint_create(s_scr, "UP/DOWN  OK:START");

    s_sel = 0;
    refresh_cards();
    eggy_battery_badge_refresh(s_battery);
    eggy_sleep_set_in_game(false);
}

void eggy_hub_exit(void) {
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
    }
}

void eggy_hub_key(const eggy_input_t *in) {
    if (in->ev != BSP_BTN_CLICK) {
        return;
    }
    if (in->btn == BSP_BTN_UP) {
        s_sel = (s_sel + EGGY_HUB_COUNT - 1) % EGGY_HUB_COUNT;
        refresh_cards();
        eggy_sfx_play(EGGY_SFX_SELECT);
    } else if (in->btn == BSP_BTN_DOWN) {
        s_sel = (s_sel + 1) % EGGY_HUB_COUNT;
        refresh_cards();
        eggy_sfx_play(EGGY_SFX_SELECT);
    } else if (in->btn == BSP_BTN_OK) {
        eggy_sfx_play(EGGY_SFX_START);
        eggy_switch_to((eggy_screen_id_t)(EGGY_SCREEN_RUN + s_sel));
    }
}
