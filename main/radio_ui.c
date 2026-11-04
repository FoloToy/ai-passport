#include "radio_ui.h"

#include "bsp_battery.h"
#include "bsp_display.h"
#include "radio_player.h"
#include "radio_stations.h"
#include "radio_wifi.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(radio_font_16);

static const char *TAG = "radio_ui";

typedef enum {
    UI_PAGE_NOW = 0,
    UI_PAGE_LIST,
    UI_PAGE_WIFI,
} ui_page_t;

static radio_settings_t *s_settings;
static ui_page_t s_page = UI_PAGE_NOW;
static size_t s_station_idx;
static size_t s_list_focus;
static bool s_vol_mode;
static int64_t s_vol_until_us;

static lv_obj_t *s_scr;
static lv_obj_t *s_title;
static lv_obj_t *s_meta;
static lv_obj_t *s_status;
static lv_obj_t *s_hint;
static lv_obj_t *s_bat;
static lv_obj_t *s_wifi_lbl;
static lv_obj_t *s_list_box;
static lv_obj_t *s_list_rows[8];
static lv_timer_t *s_timer;

static const lv_font_t *ui_font(void)
{
    return &radio_font_16;
}

static void style_label(lv_obj_t *obj, lv_color_t color)
{
    lv_obj_set_style_text_font(obj, ui_font(), 0);
    lv_obj_set_style_text_color(obj, color, 0);
}

static void refresh_battery(void)
{
    int soc = bsp_battery_soc();
    if (soc < 0) {
        lv_label_set_text(s_bat, "--%");
    } else {
        lv_label_set_text_fmt(s_bat, "%d%%", soc);
    }
}

static void show_station_meta(void)
{
    const radio_station_t *st = radio_station_at(s_station_idx);
    if (!st) {
        lv_label_set_text(s_title, "无电台");
        lv_label_set_text(s_meta, "");
        return;
    }
    lv_label_set_text(s_title, st->name_zh);
    lv_label_set_text_fmt(s_meta, "%s · %s", st->category_zh, st->region_zh);
}

static void refresh_list(void)
{
    size_t n = radio_station_count();
    size_t start = 0;
    if (s_list_focus >= 7) {
        start = s_list_focus - 6;
    }
    for (int i = 0; i < 8; i++) {
        size_t idx = start + (size_t)i;
        if (idx >= n) {
            lv_label_set_text(s_list_rows[i], "");
            continue;
        }
        const radio_station_t *st = radio_station_at(idx);
        char line[48];
        snprintf(line, sizeof(line), "%s%s", idx == s_list_focus ? "> " : "  ", st->name_zh);
        lv_label_set_text(s_list_rows[i], line);
        lv_obj_set_style_text_color(s_list_rows[i],
                                    idx == s_list_focus ? lv_color_hex(0xF0A35E)
                                                       : lv_color_hex(0xC9D7E8),
                                    0);
    }
}

static void set_page(ui_page_t page)
{
    s_page = page;
    s_vol_mode = false;
    if (page == UI_PAGE_LIST) {
        lv_obj_clear_flag(s_list_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_title, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_meta, LV_OBJ_FLAG_HIDDEN);
        s_list_focus = s_station_idx;
        refresh_list();
        lv_label_set_text(s_hint, "上/下选台  短按播放  长按返回");
        lv_label_set_text(s_status, "频道列表");
    } else if (page == UI_PAGE_WIFI) {
        lv_obj_add_flag(s_list_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_title, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_meta, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_title, "配网模式");
        lv_label_set_text_fmt(s_meta, "热点 %s", radio_wifi_softap_ssid());
        lv_label_set_text(s_status, "手机连热点后打开 192.168.4.1");
        lv_label_set_text(s_hint, "长按确定返回播放页");
    } else {
        lv_obj_add_flag(s_list_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_title, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_meta, LV_OBJ_FLAG_HIDDEN);
        show_station_meta();
        lv_label_set_text(s_hint, "上/下切台  短按暂停  长按列表");
    }
}

static void persist_station(void)
{
    const radio_station_t *st = radio_station_at(s_station_idx);
    if (!st || !s_settings) {
        return;
    }
    strncpy(s_settings->station_id, st->id, sizeof(s_settings->station_id) - 1);
    radio_nvs_save(s_settings);
}

static void play_current(void)
{
    const radio_station_t *st = radio_station_at(s_station_idx);
    if (!st) {
        return;
    }
    if (!radio_wifi_is_connected()) {
        lv_label_set_text(s_status, "未联网，请先配网");
        return;
    }
    persist_station();
    lv_label_set_text(s_status, "正在连接…");
    radio_player_play_url(st->url);
}

static void adjust_volume(int delta)
{
    if (!s_settings) {
        return;
    }
    int v = (int)s_settings->volume + delta;
    if (v < 0) {
        v = 0;
    }
    if (v > 100) {
        v = 100;
    }
    s_settings->volume = (uint8_t)v;
    radio_player_set_volume(s_settings->volume);
    radio_nvs_save(s_settings);
    s_vol_mode = true;
    s_vol_until_us = esp_timer_get_time() + 1500000;
    lv_label_set_text_fmt(s_status, "音量 %d%%", s_settings->volume);
}

static void on_tick(lv_timer_t *t)
{
    (void)t;
    refresh_battery();
    if (s_vol_mode && esp_timer_get_time() > s_vol_until_us) {
        s_vol_mode = false;
    }
    if (radio_wifi_is_connected()) {
        lv_label_set_text_fmt(s_wifi_lbl, "WiFi %s", radio_wifi_ip());
    } else if (radio_wifi_state() == RADIO_WIFI_PROVISIONING) {
        lv_label_set_text(s_wifi_lbl, "配网中");
    } else if (radio_wifi_state() == RADIO_WIFI_CONNECTING) {
        lv_label_set_text(s_wifi_lbl, "连接中");
    } else {
        lv_label_set_text(s_wifi_lbl, "未联网");
    }
}

esp_err_t radio_ui_start(radio_settings_t *settings)
{
    s_settings = settings;
    int found = radio_station_find_id(settings->station_id);
    s_station_idx = found >= 0 ? (size_t)found : 0;
    s_list_focus = s_station_idx;

    if (!bsp_lvgl_lock(500)) {
        return ESP_ERR_TIMEOUT;
    }

    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(0x07131F), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Top bar */
    lv_obj_t *brand = lv_label_create(s_scr);
    style_label(brand, lv_color_hex(0xF0A35E));
    lv_label_set_text(brand, "全球广播");
    lv_obj_set_pos(brand, 10, 8);

    s_wifi_lbl = lv_label_create(s_scr);
    style_label(s_wifi_lbl, lv_color_hex(0x8FA6BD));
    lv_obj_set_pos(s_wifi_lbl, 10, 28);
    lv_label_set_text(s_wifi_lbl, "未联网");

    s_bat = lv_label_create(s_scr);
    style_label(s_bat, lv_color_hex(0xC9D7E8));
    lv_obj_align(s_bat, LV_ALIGN_TOP_RIGHT, -10, 8);
    refresh_battery();

    /* Decorative wave line */
    lv_obj_t *line = lv_obj_create(s_scr);
    lv_obj_set_size(line, 220, 2);
    lv_obj_set_pos(line, 10, 50);
    lv_obj_set_style_bg_color(line, lv_color_hex(0x2A4A66), 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_radius(line, 0, 0);

    s_title = lv_label_create(s_scr);
    style_label(s_title, lv_color_hex(0xF5F7FA));
    lv_obj_set_width(s_title, 220);
    lv_obj_set_pos(s_title, 10, 70);
    lv_label_set_long_mode(s_title, LV_LABEL_LONG_SCROLL_CIRCULAR);

    s_meta = lv_label_create(s_scr);
    style_label(s_meta, lv_color_hex(0x8FA6BD));
    lv_obj_set_width(s_meta, 220);
    lv_obj_set_pos(s_meta, 10, 98);

    s_status = lv_label_create(s_scr);
    style_label(s_status, lv_color_hex(0xF0A35E));
    lv_obj_set_width(s_status, 220);
    lv_obj_set_pos(s_status, 10, 140);
    lv_label_set_text(s_status, "准备就绪");

    s_hint = lv_label_create(s_scr);
    style_label(s_hint, lv_color_hex(0x6D8299));
    lv_obj_set_width(s_hint, 220);
    lv_obj_set_pos(s_hint, 10, 280);
    lv_label_set_text(s_hint, "上/下切台  短按暂停  长按列表");

    s_list_box = lv_obj_create(s_scr);
    lv_obj_set_size(s_list_box, 220, 200);
    lv_obj_set_pos(s_list_box, 10, 60);
    lv_obj_set_style_bg_opa(s_list_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list_box, 0, 0);
    lv_obj_set_style_pad_all(s_list_box, 0, 0);
    lv_obj_add_flag(s_list_box, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 8; i++) {
        s_list_rows[i] = lv_label_create(s_list_box);
        style_label(s_list_rows[i], lv_color_hex(0xC9D7E8));
        lv_obj_set_pos(s_list_rows[i], 0, i * 24);
        lv_obj_set_width(s_list_rows[i], 220);
    }

    show_station_meta();
    lv_screen_load(s_scr);
    s_timer = lv_timer_create(on_tick, 1000, NULL);
    set_page(UI_PAGE_NOW);
    bsp_lvgl_unlock();

    ESP_LOGI(TAG, "UI ready station=%u", (unsigned)s_station_idx);
    return ESP_OK;
}

void radio_ui_on_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (!bsp_lvgl_lock(100)) {
        return;
    }

    if (s_page == UI_PAGE_WIFI) {
        if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
            set_page(UI_PAGE_NOW);
        }
        bsp_lvgl_unlock();
        return;
    }

    if (s_page == UI_PAGE_LIST) {
        if (ev == BSP_BTN_CLICK && btn == BSP_BTN_UP) {
            s_list_focus = radio_station_prev(s_list_focus);
            refresh_list();
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_DOWN) {
            s_list_focus = radio_station_next(s_list_focus);
            refresh_list();
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK) {
            s_station_idx = s_list_focus;
            set_page(UI_PAGE_NOW);
            play_current();
        } else if (ev == BSP_BTN_LONG && btn == BSP_BTN_OK) {
            set_page(UI_PAGE_NOW);
        } else if (ev == BSP_BTN_DOUBLE && btn == BSP_BTN_OK) {
            /* Volume mode entry from list: ignored */
        }
        bsp_lvgl_unlock();
        return;
    }

    /* NOW playing page */
    if (ev == BSP_BTN_CLICK && btn == BSP_BTN_UP) {
        if (s_vol_mode) {
            adjust_volume(5);
        } else {
            s_station_idx = radio_station_prev(s_station_idx);
            show_station_meta();
            play_current();
        }
    } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_DOWN) {
        if (s_vol_mode) {
            adjust_volume(-5);
        } else {
            s_station_idx = radio_station_next(s_station_idx);
            show_station_meta();
            play_current();
        }
    } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK) {
        radio_player_state_t st = radio_player_state();
        if (st == RADIO_PLAYER_PLAYING) {
            radio_player_pause();
            lv_label_set_text(s_status, "已暂停");
        } else if (st == RADIO_PLAYER_PAUSED) {
            radio_player_resume();
        } else {
            play_current();
        }
    } else if (ev == BSP_BTN_LONG && btn == BSP_BTN_OK) {
        set_page(UI_PAGE_LIST);
    } else if (ev == BSP_BTN_DOUBLE && btn == BSP_BTN_OK) {
        adjust_volume(0); /* enter volume display */
        lv_label_set_text(s_hint, "上/下调节音量");
    } else if (ev == BSP_BTN_LONG && btn == BSP_BTN_UP) {
        /* Force SoftAP re-provision */
        radio_player_stop();
        radio_wifi_start_softap();
        set_page(UI_PAGE_WIFI);
    }

    bsp_lvgl_unlock();
}

void radio_ui_on_wifi(radio_wifi_state_t state, const char *detail)
{
    if (!bsp_lvgl_lock(100)) {
        return;
    }
    if (state == RADIO_WIFI_CONNECTED) {
        lv_label_set_text_fmt(s_status, "已联网 %s", detail ? detail : "");
        if (s_page == UI_PAGE_WIFI) {
            set_page(UI_PAGE_NOW);
            play_current();
        }
    } else if (state == RADIO_WIFI_PROVISIONING) {
        set_page(UI_PAGE_WIFI);
        if (detail) {
            lv_label_set_text_fmt(s_meta, "热点 %s", detail);
        }
    } else if (state == RADIO_WIFI_FAILED) {
        lv_label_set_text(s_status, detail ? detail : "联网失败");
    } else if (state == RADIO_WIFI_CONNECTING) {
        lv_label_set_text_fmt(s_status, "连接 %s", detail ? detail : "Wi-Fi");
    }
    bsp_lvgl_unlock();
}

void radio_ui_on_player(radio_player_state_t state, const char *message)
{
    if (!bsp_lvgl_lock(50)) {
        return;
    }
    if (message && message[0]) {
        lv_label_set_text(s_status, message);
    } else {
        switch (state) {
        case RADIO_PLAYER_PLAYING:
            lv_label_set_text(s_status, "播放中");
            break;
        case RADIO_PLAYER_PAUSED:
            lv_label_set_text(s_status, "已暂停");
            break;
        case RADIO_PLAYER_CONNECTING:
            lv_label_set_text(s_status, "连接中…");
            break;
        case RADIO_PLAYER_BUFFERING:
            lv_label_set_text(s_status, "缓冲中…");
            break;
        case RADIO_PLAYER_ERROR:
            lv_label_set_text(s_status, "播放失败");
            break;
        default:
            lv_label_set_text(s_status, "已停止");
            break;
        }
    }
    bsp_lvgl_unlock();
}
