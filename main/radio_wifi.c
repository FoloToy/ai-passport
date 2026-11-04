#include "radio_wifi.h"

#include "dns_server.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "radio_wifi";

static radio_wifi_cb_t s_cb;
static void *s_user;
static volatile radio_wifi_state_t s_state = RADIO_WIFI_IDLE;
static char s_ip[16];
static char s_ap_ssid[32];
static esp_netif_t *s_sta;
static esp_netif_t *s_ap;
static httpd_handle_t s_httpd;
static dns_server_handle_t s_dns;
static bool s_wifi_started;
static bool s_allow_sta_connect;
static EventGroupHandle_t s_events;
static const int GOT_IP_BIT = BIT0;
static const int FAIL_BIT = BIT1;

static void notify(radio_wifi_state_t st, const char *detail)
{
    s_state = st;
    if (s_cb) {
        s_cb(st, detail, s_user);
    }
}

static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if (s_allow_sta_connect) {
            esp_wifi_connect();
        }
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupSetBits(s_events, FAIL_BIT);
        if (s_state != RADIO_WIFI_PROVISIONING) {
            notify(RADIO_WIFI_FAILED, "Wi-Fi 断开");
        }
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *ev = (wifi_event_ap_staconnected_t *)data;
        ESP_LOGI(TAG, "phone joined SoftAP " MACSTR, MAC2STR(ev->mac));
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *ev = (wifi_event_ap_stadisconnected_t *)data;
        ESP_LOGI(TAG, "phone left SoftAP " MACSTR " reason=%d", MAC2STR(ev->mac), ev->reason);
    } else if (base == IP_EVENT && id == IP_EVENT_AP_STAIPASSIGNED) {
        ESP_LOGI(TAG, "DHCP assigned phone an IP");
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_events, GOT_IP_BIT);
        notify(RADIO_WIFI_CONNECTED, s_ip);
    }
}

static esp_err_t root_get(httpd_req_t *req)
{
    static const char *page =
        "<!DOCTYPE html><html><head><meta charset=utf-8>"
        "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
        "<title>GlobalRadio Setup</title>"
        "<style>body{font-family:sans-serif;background:#0b1c2c;color:#e8f2ff;padding:24px}"
        "input,button{width:100%;box-sizing:border-box;padding:12px;margin:8px 0;border-radius:8px;border:0}"
        "button{background:#f0a35e;color:#1a1008;font-weight:700}</style></head><body>"
        "<h2>Global Broadcast Setup</h2>"
        "<p>Enter your <b>2.4 GHz</b> Wi-Fi (5 GHz will not work).</p>"
        "<form method=POST action=/save>"
        "<label>Wi-Fi name (SSID)</label><input name=ssid maxlength=32 required autocomplete=off>"
        "<label>Password</label><input name=pass type=password maxlength=64>"
        "<button type=submit>Connect &amp; Save</button></form>"
        "<p style=opacity:.7>After success the hotspot closes. Return to the device.</p>"
        "</body></html>";
    ESP_LOGI(TAG, "HTTP GET %s", req->uri);
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
}

static void url_decode(char *s)
{
    char *o = s;
    for (char *p = s; *p; p++) {
        if (*p == '+') {
            *o++ = ' ';
        } else if (*p == '%' && p[1] && p[2]) {
            char hex[3] = {p[1], p[2], 0};
            *o++ = (char)strtol(hex, NULL, 16);
            p += 2;
        } else {
            *o++ = *p;
        }
    }
    *o = '\0';
}

static esp_err_t save_post(httpd_req_t *req)
{
    char body[192];
    int total = req->content_len;
    if (total <= 0 || total >= (int)sizeof(body)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad length");
        return ESP_FAIL;
    }
    int got = 0;
    while (got < total) {
        int n = httpd_req_recv(req, body + got, total - got);
        if (n <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv");
            return ESP_FAIL;
        }
        got += n;
    }
    body[got] = '\0';

    char ssid[RADIO_SSID_MAX] = {0};
    char pass[RADIO_PASS_MAX] = {0};
    char *tok = strtok(body, "&");
    while (tok) {
        if (strncmp(tok, "ssid=", 5) == 0) {
            strncpy(ssid, tok + 5, sizeof(ssid) - 1);
            url_decode(ssid);
        } else if (strncmp(tok, "pass=", 5) == 0) {
            strncpy(pass, tok + 5, sizeof(pass) - 1);
            url_decode(pass);
        }
        tok = strtok(NULL, "&");
    }
    if (!ssid[0]) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "ssid required");
        return ESP_FAIL;
    }

    radio_settings_t settings;
    radio_nvs_load(&settings);
    strncpy(settings.ssid, ssid, sizeof(settings.ssid) - 1);
    strncpy(settings.password, pass, sizeof(settings.password) - 1);
    settings.has_wifi = true;

    notify(RADIO_WIFI_CONNECTING, "验证中…");
    wifi_config_t cfg = {0};
    strncpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid));
    strncpy((char *)cfg.sta.password, pass, sizeof(cfg.sta.password));
    cfg.sta.threshold.authmode = pass[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    /* Keep SoftAP up while we validate STA so the phone can still see the result. */
    s_allow_sta_connect = true;
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    xEventGroupClearBits(s_events, GOT_IP_BIT | FAIL_BIT);
    esp_wifi_connect();

    EventBits_t bits = xEventGroupWaitBits(s_events, GOT_IP_BIT | FAIL_BIT, pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(20000));
    if (bits & GOT_IP_BIT) {
        radio_nvs_save(&settings);
        const char *ok =
            "<!DOCTYPE html><html><body style='font-family:sans-serif;background:#0b1c2c;color:#e8f2ff;padding:24px'>"
            "<h2>Connected</h2><p>Hotspot will close. Look at the device screen.</p></body></html>";
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, ok, HTTPD_RESP_USE_STRLEN);
        radio_wifi_stop_softap();
        notify(RADIO_WIFI_CONNECTED, s_ip);
        return ESP_OK;
    }

    /* Stay in SoftAP so the user can retry. */
    s_allow_sta_connect = false;
    esp_wifi_set_mode(WIFI_MODE_AP);
    const char *fail =
        "<!DOCTYPE html><html><body style='font-family:sans-serif;background:#0b1c2c;color:#e8f2ff;padding:24px'>"
        "<h2>Connect failed</h2><p>Use a 2.4 GHz network and check the password.</p>"
        "<p><a style=color:#f0a35e href=/>Back</a></p></body></html>";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, fail, HTTPD_RESP_USE_STRLEN);
    notify(RADIO_WIFI_PROVISIONING, "验证失败，可重试");
    return ESP_OK;
}

static esp_err_t http_404_handler(httpd_req_t *req, httpd_err_code_t err)
{
    (void)err;
    /* iOS captive portal needs body content, not only a redirect. */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, "Redirect to captive portal", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t start_httpd(void)
{
    if (s_httpd) {
        return ESP_OK;
    }
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    /* Keep sockets modest on no-PSRAM C3, but high enough for phone probes. */
    config.max_open_sockets = 7;
    config.lru_purge_enable = true;
    config.stack_size = 6144;
    esp_err_t err = httpd_start(&s_httpd, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start: %s", esp_err_to_name(err));
        return err;
    }
    const httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = root_get};
    const httpd_uri_t save = {.uri = "/save", .method = HTTP_POST, .handler = save_post};
    httpd_register_uri_handler(s_httpd, &root);
    httpd_register_uri_handler(s_httpd, &save);
    httpd_register_err_handler(s_httpd, HTTPD_404_NOT_FOUND, http_404_handler);
    ESP_LOGI(TAG, "httpd listening on :80");
    return ESP_OK;
}

static void start_dns_redirect(void)
{
    if (s_dns) {
        return;
    }
    dns_server_config_t cfg = DNS_SERVER_CONFIG_SINGLE("*", "WIFI_AP_DEF");
    s_dns = start_dns_server(&cfg);
    if (!s_dns) {
        ESP_LOGW(TAG, "DNS captive redirect failed to start");
    } else {
        ESP_LOGI(TAG, "DNS captive redirect started");
    }
}

esp_err_t radio_wifi_init(radio_wifi_cb_t cb, void *user)
{
    s_cb = cb;
    s_user = user;
    s_events = xEventGroupCreate();
    if (!s_events) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        return err;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta = esp_netif_create_default_wifi_sta();
    s_ap = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, on_wifi, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    /* Captive probes are noisy. */
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);
    return ESP_OK;
}

esp_err_t radio_wifi_connect_saved(const radio_settings_t *settings)
{
    if (!settings || !settings->has_wifi || !settings->ssid[0]) {
        return ESP_ERR_INVALID_ARG;
    }
    notify(RADIO_WIFI_CONNECTING, settings->ssid);
    wifi_config_t cfg = {0};
    strncpy((char *)cfg.sta.ssid, settings->ssid, sizeof(cfg.sta.ssid));
    strncpy((char *)cfg.sta.password, settings->password, sizeof(cfg.sta.password));
    if (settings->password[0]) {
        cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }
    s_allow_sta_connect = true;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    if (!s_wifi_started) {
        ESP_ERROR_CHECK(esp_wifi_start());
        s_wifi_started = true;
    } else {
        esp_wifi_connect();
    }
    EventBits_t bits = xEventGroupWaitBits(s_events, GOT_IP_BIT | FAIL_BIT, pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(20000));
    if (bits & GOT_IP_BIT) {
        return ESP_OK;
    }
    notify(RADIO_WIFI_FAILED, "无法连接已存 Wi-Fi");
    return ESP_FAIL;
}

esp_err_t radio_wifi_start_softap(void)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    /* ASCII SSID: phones often hide UTF-8 Chinese SoftAP names. */
    snprintf(s_ap_ssid, sizeof(s_ap_ssid), "GlobalRadio-%02X%02X", mac[4], mac[5]);

    wifi_config_t ap = {0};
    strncpy((char *)ap.ap.ssid, s_ap_ssid, sizeof(ap.ap.ssid));
    ap.ap.ssid_len = strlen(s_ap_ssid);
    ap.ap.max_connection = 4;
    ap.ap.authmode = WIFI_AUTH_OPEN;
    ap.ap.channel = 6;

    /* AP-only while provisioning: more RAM/sockets, no empty STA connect. */
    s_allow_sta_connect = false;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    if (!s_wifi_started) {
        ESP_ERROR_CHECK(esp_wifi_start());
        s_wifi_started = true;
    }

    esp_err_t err = start_httpd();
    if (err != ESP_OK) {
        return err;
    }
    start_dns_redirect();

    notify(RADIO_WIFI_PROVISIONING, s_ap_ssid);
    ESP_LOGI(TAG, "SoftAP %s portal http://192.168.4.1/", s_ap_ssid);
    return ESP_OK;
}

void radio_wifi_stop_softap(void)
{
    if (s_dns) {
        stop_dns_server(s_dns);
        s_dns = NULL;
    }
    if (s_httpd) {
        httpd_stop(s_httpd);
        s_httpd = NULL;
    }
    s_allow_sta_connect = true;
    esp_wifi_set_mode(WIFI_MODE_STA);
}

bool radio_wifi_is_connected(void)
{
    return s_state == RADIO_WIFI_CONNECTED;
}

radio_wifi_state_t radio_wifi_state(void)
{
    return s_state;
}

const char *radio_wifi_ip(void)
{
    return s_ip;
}

const char *radio_wifi_softap_ssid(void)
{
    return s_ap_ssid;
}
