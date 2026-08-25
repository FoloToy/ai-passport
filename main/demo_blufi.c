// BLUFI over NimBLE: receive Wi-Fi STA credentials from EspBlufi mobile apps.
#include "demo.h"
#include "demo_blufi_security.h"
#include "demo_radio.h"
#include "ui_pixel.h"

#include "esp_blufi.h"
#include "esp_blufi_api.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "host/ble_hs.h"
#include "lwip/ip4_addr.h"
#include "lvgl.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include <string.h>

static const char *TAG = "demo_blufi";
// ESP Config 微信小程序默认只显示以 "BLUFI" 开头的设备。
static const char *DEVICE_NAME = "BLUFI_FoloPassport";

#define BLUFI_AP_LIST_COUNT 16

typedef enum {
    BLUFI_DEMO_OFF = 0,
    BLUFI_DEMO_STARTING,
    BLUFI_DEMO_ADVERTISING,
    BLUFI_DEMO_BLE_CONNECTED,
    BLUFI_DEMO_WIFI_CONNECTING,
    BLUFI_DEMO_WIFI_CONNECTED,
    BLUFI_DEMO_FAILED,
} blufi_demo_state_t;

static lv_obj_t *s_scr;
static lv_obj_t *s_status;
static lv_timer_t *s_timer;
static esp_netif_t *s_sta_netif;
static esp_event_handler_instance_t s_wifi_handler;
static esp_event_handler_instance_t s_ip_handler;
static wifi_config_t s_sta_config;
static volatile blufi_demo_state_t s_state;
static volatile esp_err_t s_error;
static char s_ip[16];
static bool s_wifi_initialized;
static bool s_wifi_started;
static bool s_wifi_handler_registered;
static bool s_ip_handler_registered;
static bool s_host_initialized;
static bool s_host_running;
static bool s_gatt_initialized;
static bool s_btc_initialized;
static bool s_profile_initialized;
static bool s_ble_connected;
static bool s_wifi_connecting;
static bool s_wifi_got_ip;
static bool s_reconnect_after_disconnect;
static SemaphoreHandle_t s_host_stopped;

static void send_wifi_report(esp_blufi_sta_conn_state_t state)
{
    wifi_mode_t mode = WIFI_MODE_STA;
    esp_wifi_get_mode(&mode);
    esp_blufi_extra_info_t info = { 0 };
    size_t ssid_len = strnlen((const char *)s_sta_config.sta.ssid,
                              sizeof(s_sta_config.sta.ssid));
    if (ssid_len > 0) {
        info.sta_ssid = s_sta_config.sta.ssid;
        info.sta_ssid_len = ssid_len;
    }
    esp_blufi_send_wifi_conn_report(mode, state, 0, &info);
}

static void send_wifi_list(void)
{
    uint16_t count = BLUFI_AP_LIST_COUNT;
    wifi_ap_record_t records[BLUFI_AP_LIST_COUNT] = { 0 };
    esp_blufi_ap_record_t list[BLUFI_AP_LIST_COUNT] = { 0 };
    esp_err_t err = esp_wifi_scan_get_ap_records(&count, records);
    if (err != ESP_OK) {
        esp_blufi_send_error_info(ESP_BLUFI_WIFI_SCAN_FAIL);
        return;
    }
    for (uint16_t i = 0; i < count; i++) {
        list[i].rssi = records[i].rssi;
        memcpy(list[i].ssid, records[i].ssid, sizeof(list[i].ssid));
    }
    if (s_ble_connected) esp_blufi_send_wifi_list(count, list);
}

static void request_wifi_connect(void)
{
    bool was_connected = s_wifi_got_ip;
    s_wifi_connecting = true;
    s_wifi_got_ip = false;
    s_state = BLUFI_DEMO_WIFI_CONNECTING;
    if (was_connected) {
        s_reconnect_after_disconnect = true;
        if (esp_wifi_disconnect() == ESP_OK) return;
        s_reconnect_after_disconnect = false;
    }
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        s_error = err;
        s_wifi_connecting = false;
        s_state = BLUFI_DEMO_FAILED;
    }
}

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    if (id == WIFI_EVENT_STA_START) {
        esp_wifi_get_config(WIFI_IF_STA, &s_sta_config);
        if (s_sta_config.sta.ssid[0] != '\0') {
            s_wifi_connecting = true;
            s_state = BLUFI_DEMO_WIFI_CONNECTING;
            esp_wifi_connect();
        }
    } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = data;
        ESP_LOGW(TAG, "Wi-Fi disconnected, reason=%u", event->reason);
        if (s_reconnect_after_disconnect) {
            s_reconnect_after_disconnect = false;
            esp_wifi_connect();
            return;
        }
        if (s_state == BLUFI_DEMO_WIFI_CONNECTING) {
            send_wifi_report(ESP_BLUFI_STA_CONN_FAIL);
        }
        s_wifi_connecting = false;
        s_wifi_got_ip = false;
        s_state = s_ble_connected ? BLUFI_DEMO_BLE_CONNECTED : BLUFI_DEMO_ADVERTISING;
        s_ip[0] = '\0';
    } else if (id == WIFI_EVENT_SCAN_DONE) {
        send_wifi_list();
    }
}

static void ip_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    if (id != IP_EVENT_STA_GOT_IP) return;
    ip_event_got_ip_t *event = data;
    snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&event->ip_info.ip));
    s_wifi_connecting = false;
    s_wifi_got_ip = true;
    s_state = BLUFI_DEMO_WIFI_CONNECTED;
    if (s_ble_connected) send_wifi_report(ESP_BLUFI_STA_CONN_SUCCESS);
}

static void blufi_reset(int reason)
{
    ESP_LOGE(TAG, "NimBLE reset: %d", reason);
    s_error = ESP_FAIL;
    s_state = BLUFI_DEMO_FAILED;
}

static void blufi_sync(void)
{
    int rc = esp_blufi_profile_init();
    if (rc == 0) {
        s_profile_initialized = true;
    } else {
        s_error = rc;
        s_state = BLUFI_DEMO_FAILED;
    }
}

static void host_task(void *arg)
{
    (void)arg;
    nimble_port_run();
    if (s_host_stopped) xSemaphoreGive(s_host_stopped);
    nimble_port_freertos_deinit();
}

static void blufi_event(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param)
{
    switch (event) {
    case ESP_BLUFI_EVENT_INIT_FINISH:
        esp_blufi_adv_start_with_name(DEVICE_NAME);
        if (s_wifi_got_ip) {
            s_state = BLUFI_DEMO_WIFI_CONNECTED;
        } else if (s_wifi_connecting) {
            s_state = BLUFI_DEMO_WIFI_CONNECTING;
        } else {
            s_state = BLUFI_DEMO_ADVERTISING;
        }
        break;
    case ESP_BLUFI_EVENT_BLE_CONNECT:
        s_ble_connected = true;
        esp_blufi_adv_stop();
        if (demo_blufi_security_init() != 0) {
            s_error = ESP_ERR_NO_MEM;
            s_state = BLUFI_DEMO_FAILED;
        } else if (!s_wifi_got_ip) {
            s_state = BLUFI_DEMO_BLE_CONNECTED;
        }
        break;
    case ESP_BLUFI_EVENT_BLE_DISCONNECT:
        s_ble_connected = false;
        demo_blufi_security_deinit();
        esp_blufi_adv_start_with_name(DEVICE_NAME);
        s_state = s_wifi_got_ip ? BLUFI_DEMO_WIFI_CONNECTED : BLUFI_DEMO_ADVERTISING;
        break;
    case ESP_BLUFI_EVENT_SET_WIFI_OPMODE:
        esp_wifi_set_mode(WIFI_MODE_STA);
        break;
    case ESP_BLUFI_EVENT_RECV_STA_BSSID:
        memcpy(s_sta_config.sta.bssid, param->sta_bssid.bssid, 6);
        s_sta_config.sta.bssid_set = true;
        esp_wifi_set_config(WIFI_IF_STA, &s_sta_config);
        break;
    case ESP_BLUFI_EVENT_RECV_STA_SSID:
        if (param->sta_ssid.ssid_len >= sizeof(s_sta_config.sta.ssid)) {
            esp_blufi_send_error_info(ESP_BLUFI_DATA_FORMAT_ERROR);
            break;
        }
        memset(s_sta_config.sta.ssid, 0, sizeof(s_sta_config.sta.ssid));
        memset(s_sta_config.sta.password, 0, sizeof(s_sta_config.sta.password));
        memset(s_sta_config.sta.bssid, 0, sizeof(s_sta_config.sta.bssid));
        s_sta_config.sta.bssid_set = false;
        s_sta_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
        memcpy(s_sta_config.sta.ssid, param->sta_ssid.ssid,
               param->sta_ssid.ssid_len);
        esp_wifi_set_config(WIFI_IF_STA, &s_sta_config);
        break;
    case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
        if (param->sta_passwd.passwd_len >= sizeof(s_sta_config.sta.password)) {
            esp_blufi_send_error_info(ESP_BLUFI_DATA_FORMAT_ERROR);
            break;
        }
        memset(s_sta_config.sta.password, 0, sizeof(s_sta_config.sta.password));
        memcpy(s_sta_config.sta.password, param->sta_passwd.passwd,
               param->sta_passwd.passwd_len);
        esp_wifi_set_config(WIFI_IF_STA, &s_sta_config);
        break;
    case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
        request_wifi_connect();
        break;
    case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
        esp_wifi_disconnect();
        break;
    case ESP_BLUFI_EVENT_GET_WIFI_STATUS:
        if (s_wifi_got_ip) {
            send_wifi_report(ESP_BLUFI_STA_CONN_SUCCESS);
        } else if (s_wifi_connecting) {
            send_wifi_report(ESP_BLUFI_STA_CONNECTING);
        } else {
            send_wifi_report(ESP_BLUFI_STA_CONN_FAIL);
        }
        break;
    case ESP_BLUFI_EVENT_GET_WIFI_LIST: {
        wifi_scan_config_t config = { 0 };
        if (esp_wifi_scan_start(&config, false) != ESP_OK) {
            esp_blufi_send_error_info(ESP_BLUFI_WIFI_SCAN_FAIL);
        }
        break;
    }
    case ESP_BLUFI_EVENT_RECV_SLAVE_DISCONNECT_BLE:
        esp_blufi_disconnect();
        break;
    case ESP_BLUFI_EVENT_DEAUTHENTICATE_STA:
        esp_wifi_disconnect();
        break;
    case ESP_BLUFI_EVENT_REPORT_ERROR:
        esp_blufi_send_error_info(param->report_error.state);
        break;
    default:
        break;
    }
}

static esp_blufi_callbacks_t s_callbacks = {
    .event_cb = blufi_event,
    .negotiate_data_handler = demo_blufi_negotiate,
    .encrypt_func = demo_blufi_encrypt,
    .decrypt_func = demo_blufi_decrypt,
    .checksum_func = demo_blufi_checksum,
};

static esp_err_t wifi_start(void)
{
    esp_err_t err = demo_radio_nvs_prepare();
    if (err != ESP_OK) return err;
    err = demo_radio_network_prepare();
    if (err != ESP_OK) return err;

    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (!s_sta_netif) return ESP_ERR_NO_MEM;
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&config);
    if (err != ESP_OK) return err;
    s_wifi_initialized = true;
    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              wifi_event, NULL, &s_wifi_handler);
    if (err != ESP_OK) return err;
    s_wifi_handler_registered = true;
    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              ip_event, NULL, &s_ip_handler);
    if (err != ESP_OK) return err;
    s_ip_handler_registered = true;
    err = esp_wifi_set_storage(WIFI_STORAGE_FLASH);
    if (err == ESP_OK) err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err == ESP_OK) err = esp_wifi_start();
    if (err == ESP_OK) s_wifi_started = true;
    return err;
}

static esp_err_t host_start(void)
{
    esp_err_t err = esp_blufi_register_callbacks(&s_callbacks);
    if (err != ESP_OK) return err;
    err = nimble_port_init();
    if (err != ESP_OK) return err;
    s_host_initialized = true;
    s_host_stopped = xSemaphoreCreateBinary();
    if (!s_host_stopped) return ESP_ERR_NO_MEM;
    ble_hs_cfg.reset_cb = blufi_reset;
    ble_hs_cfg.sync_cb = blufi_sync;
    ble_hs_cfg.gatts_register_cb = esp_blufi_gatt_svr_register_cb;
    int rc = esp_blufi_gatt_svr_init();
    if (rc != 0) return ESP_FAIL;
    s_gatt_initialized = true;
    rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    if (rc != 0) return ESP_FAIL;
    esp_blufi_btc_init();
    s_btc_initialized = true;
    err = esp_nimble_enable(host_task);
    if (err == ESP_OK) s_host_running = true;
    return err;
}

static void demo_stop(void)
{
    s_ble_connected = false;
    demo_blufi_security_deinit();
    if (s_host_initialized) {
        if (s_profile_initialized) esp_blufi_adv_stop();
        if (s_gatt_initialized) {
            esp_blufi_gatt_svr_deinit();
            s_gatt_initialized = false;
        }
        bool host_stopped = !s_host_running;
        if (s_host_running) {
            int rc = nimble_port_stop();
            if (rc == 0) {
                xSemaphoreTake(s_host_stopped, portMAX_DELAY);
                host_stopped = true;
            } else {
                ESP_LOGE(TAG, "nimble_port_stop failed: %d", rc);
            }
        }
        if (host_stopped) nimble_port_deinit();
        s_host_running = false;
        if (s_profile_initialized) {
            esp_blufi_profile_deinit();
            s_profile_initialized = false;
        }
        if (s_btc_initialized) {
            esp_blufi_btc_deinit();
            s_btc_initialized = false;
        }
        s_host_initialized = false;
    }
    if (s_host_stopped) {
        vSemaphoreDelete(s_host_stopped);
        s_host_stopped = NULL;
    }
    if (s_wifi_started) {
        esp_wifi_scan_stop();
        esp_wifi_disconnect();
        esp_wifi_stop();
        s_wifi_started = false;
    }
    if (s_ip_handler_registered) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              s_ip_handler);
        s_ip_handler_registered = false;
    }
    if (s_wifi_handler_registered) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              s_wifi_handler);
        s_wifi_handler_registered = false;
    }
    if (s_wifi_initialized) {
        esp_wifi_deinit();
        s_wifi_initialized = false;
    }
    if (s_sta_netif) {
        esp_netif_destroy_default_wifi(s_sta_netif);
        s_sta_netif = NULL;
    }
    s_state = BLUFI_DEMO_OFF;
}

static void tick(lv_timer_t *timer)
{
    (void)timer;
    switch (s_state) {
    case BLUFI_DEMO_STARTING:
        lv_label_set_text(s_status, "Starting Wi-Fi + BLUFI...");
        break;
    case BLUFI_DEMO_ADVERTISING:
        lv_label_set_text_fmt(s_status,
                              "READY FOR PROVISIONING\n\n%s\n\nOpen EspBlufi app\n\nDOWN: CLEAR WIFI",
                              DEVICE_NAME);
        break;
    case BLUFI_DEMO_BLE_CONNECTED:
        lv_label_set_text(s_status, "PHONE CONNECTED\n\nSend Wi-Fi credentials\nin EspBlufi app");
        break;
    case BLUFI_DEMO_WIFI_CONNECTING:
        lv_label_set_text_fmt(s_status, "CONNECTING WI-FI\n\nSSID: %.24s",
                              s_sta_config.sta.ssid);
        break;
    case BLUFI_DEMO_WIFI_CONNECTED:
        lv_label_set_text_fmt(s_status, "WI-FI CONNECTED\n\nSSID: %.24s\nIP: %s\n\nOK: RECONNECT",
                              s_sta_config.sta.ssid, s_ip);
        break;
    case BLUFI_DEMO_FAILED:
        lv_label_set_text_fmt(s_status, "BLUFI failed: %s", esp_err_to_name(s_error));
        s_state = BLUFI_DEMO_OFF;
        break;
    default:
        break;
    }
}

void demo_blufi_enter(void)
{
    s_scr = ui_pixel_screen_create("BLUFI SETUP");
    lv_obj_t *panel = ui_pixel_panel_create(s_scr, 18, 55, 204, 188, UI_PAPER);
    s_status = lv_label_create(panel);
    lv_obj_set_width(s_status, 176);
    lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_status, lv_color_hex(UI_INK), 0);
    lv_obj_center(s_status);
    lv_label_set_text(s_status, "Starting Wi-Fi + BLUFI...");
    ui_pixel_mascot_create(s_scr, 101, 246);
    s_timer = lv_timer_create(tick, 100, NULL);
    lv_screen_load(s_scr);

    s_ip[0] = '\0';
    s_wifi_connecting = false;
    s_wifi_got_ip = false;
    s_reconnect_after_disconnect = false;
    s_state = BLUFI_DEMO_STARTING;
    esp_err_t err = wifi_start();
    if (err == ESP_OK) err = host_start();
    if (err != ESP_OK) {
        s_error = err;
        s_state = BLUFI_DEMO_FAILED;
        ESP_LOGE(TAG, "BLUFI start failed: %s", esp_err_to_name(err));
    }
}

void demo_blufi_exit(void)
{
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    demo_stop();
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
        s_status = NULL;
    }
}

void demo_blufi_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_CLICK || !s_wifi_started) return;
    if (btn == BSP_BTN_OK) {
        request_wifi_connect();
    } else if (btn == BSP_BTN_DOWN) {
        wifi_config_t empty = { 0 };
        s_reconnect_after_disconnect = false;
        esp_wifi_disconnect();
        esp_wifi_set_config(WIFI_IF_STA, &empty);
        memset(&s_sta_config, 0, sizeof(s_sta_config));
        s_wifi_connecting = false;
        s_wifi_got_ip = false;
        s_ip[0] = '\0';
        s_state = s_ble_connected ? BLUFI_DEMO_BLE_CONNECTED : BLUFI_DEMO_ADVERTISING;
    }
}
