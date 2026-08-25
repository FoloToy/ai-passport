// main/demo_wifi.c —— STA 模式扫描附近 AP，不连接网络、不保存凭证。
#include "demo.h"
#include "demo_radio.h"
#include "ui_pixel.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "demo_wifi";

#define WIFI_RESULT_COUNT 5

typedef enum {
    WIFI_DEMO_OFF = 0,
    WIFI_DEMO_STARTING,
    WIFI_DEMO_SCANNING,
    WIFI_DEMO_READY,
    WIFI_DEMO_FAILED,
} wifi_demo_state_t;

static lv_obj_t *s_scr;
static lv_obj_t *s_status;
static lv_obj_t *s_results;
static lv_timer_t *s_timer;
static esp_netif_t *s_sta_netif;
static esp_event_handler_instance_t s_scan_handler;
static volatile wifi_demo_state_t s_state;
static volatile esp_err_t s_error;
static bool s_wifi_initialized;
static bool s_wifi_started;
static bool s_handler_registered;

static void scan_done(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    (void)id;
    (void)data;
    if (s_state == WIFI_DEMO_SCANNING) s_state = WIFI_DEMO_READY;
}

static esp_err_t start_scan(void)
{
    if (!s_wifi_started) return ESP_ERR_INVALID_STATE;

    esp_err_t err = esp_wifi_scan_start(NULL, false);
    if (err == ESP_OK) {
        s_state = WIFI_DEMO_SCANNING;
    } else {
        s_error = err;
        s_state = WIFI_DEMO_FAILED;
    }
    return err;
}

static esp_err_t wifi_start(void)
{
    s_state = WIFI_DEMO_STARTING;
    esp_err_t err = demo_radio_nvs_prepare();
    if (err != ESP_OK) goto fail;
    err = demo_radio_network_prepare();
    if (err != ESP_OK) goto fail;

    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (!s_sta_netif) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) goto fail;
    s_wifi_initialized = true;

    err = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE,
                                              scan_done, NULL, &s_scan_handler);
    if (err != ESP_OK) goto fail;
    s_handler_registered = true;

    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) goto fail;
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) goto fail;
    err = esp_wifi_start();
    if (err != ESP_OK) goto fail;
    s_wifi_started = true;

    return start_scan();

fail:
    s_error = err;
    s_state = WIFI_DEMO_FAILED;
    ESP_LOGE(TAG, "Wi-Fi 初始化失败: %s", esp_err_to_name(err));
    return err;
}

static void show_scan_results(void)
{
    uint16_t total = 0;
    uint16_t count = WIFI_RESULT_COUNT;
    wifi_ap_record_t records[WIFI_RESULT_COUNT] = { 0 };
    char text[320] = { 0 };
    size_t used = 0;

    esp_err_t err = esp_wifi_scan_get_ap_num(&total);
    if (err == ESP_OK) err = esp_wifi_scan_get_ap_records(&count, records);
    if (err != ESP_OK) {
        s_error = err;
        s_state = WIFI_DEMO_FAILED;
        return;
    }

    for (uint16_t i = 0; i < count && used < sizeof(text); i++) {
        int written = snprintf(text + used, sizeof(text) - used,
                               "%d  %.18s  ch%u\n",
                               records[i].rssi, (const char *)records[i].ssid,
                               records[i].primary);
        if (written < 0 || (size_t)written >= sizeof(text) - used) break;
        used += (size_t)written;
    }
    if (count == 0) snprintf(text, sizeof(text), "No access points found");

    lv_label_set_text_fmt(s_status, "%u APs  |  OK: RESCAN", total);
    lv_label_set_text(s_results, text);
    s_state = WIFI_DEMO_OFF;
}

static void tick(lv_timer_t *timer)
{
    (void)timer;
    switch (s_state) {
    case WIFI_DEMO_STARTING:
        lv_label_set_text(s_status, "Starting Wi-Fi...");
        break;
    case WIFI_DEMO_SCANNING:
        lv_label_set_text(s_status, "Scanning 2.4 GHz...");
        break;
    case WIFI_DEMO_READY:
        show_scan_results();
        break;
    case WIFI_DEMO_FAILED:
        lv_label_set_text_fmt(s_status, "Wi-Fi failed: %s", esp_err_to_name(s_error));
        s_state = WIFI_DEMO_OFF;
        break;
    default:
        break;
    }
}

static void wifi_stop(void)
{
    if (s_wifi_started) {
        esp_wifi_scan_stop();
        esp_wifi_stop();
        s_wifi_started = false;
    }
    if (s_handler_registered) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_SCAN_DONE,
                                              s_scan_handler);
        s_handler_registered = false;
    }
    if (s_wifi_initialized) {
        esp_wifi_deinit();
        s_wifi_initialized = false;
    }
    if (s_sta_netif) {
        esp_netif_destroy_default_wifi(s_sta_netif);
        s_sta_netif = NULL;
    }
    s_state = WIFI_DEMO_OFF;
}

void demo_wifi_enter(void)
{
    s_scr = ui_pixel_screen_create("WI-FI SCAN");
    lv_obj_t *panel = ui_pixel_panel_create(s_scr, 12, 54, 216, 190, UI_PAPER);

    s_status = lv_label_create(panel);
    lv_obj_set_width(s_status, 190);
    lv_obj_set_style_text_color(s_status, lv_color_hex(UI_SKY_DARK), 0);
    lv_obj_align(s_status, LV_ALIGN_TOP_LEFT, 2, 2);
    lv_label_set_text(s_status, "Starting Wi-Fi...");

    s_results = lv_label_create(panel);
    lv_obj_set_width(s_results, 190);
    lv_obj_set_style_text_font(s_results, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_results, lv_color_hex(UI_INK), 0);
    lv_obj_align(s_results, LV_ALIGN_TOP_LEFT, 2, 35);
    lv_label_set_text(s_results, "RSSI  SSID  CHANNEL");

    ui_pixel_mascot_create(s_scr, 101, 246);
    s_timer = lv_timer_create(tick, 100, NULL);
    lv_screen_load(s_scr);
    wifi_start();
}

void demo_wifi_exit(void)
{
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    wifi_stop();
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
        s_status = s_results = NULL;
    }
}

void demo_wifi_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (btn != BSP_BTN_OK || ev != BSP_BTN_CLICK || s_state != WIFI_DEMO_OFF) return;
    lv_label_set_text(s_results, "RSSI  SSID  CHANNEL");
    start_scan();
}
