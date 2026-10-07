#include "eevee_config.h"
#include <string.h>
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "eevee_cfg";
#define NVS_NAMESPACE "eevee_cfg"

void eevee_config_reset_defaults(eevee_config_t *cfg)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    strncpy(cfg->wifi_ssid, EEVEE_DEFAULT_WIFI_SSID, sizeof(cfg->wifi_ssid) - 1);
    strncpy(cfg->wifi_pass, EEVEE_DEFAULT_WIFI_PASS, sizeof(cfg->wifi_pass) - 1);
    strncpy(cfg->base_url, EEVEE_DEFAULT_BASE_URL, sizeof(cfg->base_url) - 1);
    strncpy(cfg->pat_token, EEVEE_DEFAULT_PAT_TOKEN, sizeof(cfg->pat_token) - 1);
    strncpy(cfg->app_id, EEVEE_DEFAULT_APP_ID, sizeof(cfg->app_id) - 1);
    cfg->poll_interval_sec = 30;
}

bool eevee_config_has_wifi(const eevee_config_t *cfg)
{
    if (!cfg) return false;
    if (cfg->wifi_ssid[0] == '\0') return false;
    if (strcmp(cfg->wifi_ssid, "Your_WiFi_SSID") == 0) return false;
    return true;
}

void eevee_config_get_web_url(const eevee_config_t *cfg, char *out_buf, size_t max_len)
{
    if (!out_buf || max_len == 0) return;
    const char *base = (cfg && cfg->base_url[0]) ? cfg->base_url : EEVEE_DEFAULT_BASE_URL;
    strncpy(out_buf, base, max_len - 1);
    out_buf[max_len - 1] = '\0';

    char *port_pos = strstr(out_buf, ":3001");
    if (port_pos) {
        port_pos[4] = '0'; // 将 :3001 替换为 :3000 (Web 前端)
    }
}

void eevee_config_init(eevee_config_t *cfg)
{
    if (!cfg) return;
    eevee_config_reset_defaults(cfg);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No NVS configuration found, using defaults");
        return;
    }

    size_t len = sizeof(cfg->wifi_ssid);
    nvs_get_str(handle, "wifi_ssid", cfg->wifi_ssid, &len);

    len = sizeof(cfg->wifi_pass);
    nvs_get_str(handle, "wifi_pass", cfg->wifi_pass, &len);

    len = sizeof(cfg->base_url);
    nvs_get_str(handle, "base_url", cfg->base_url, &len);

    len = sizeof(cfg->pat_token);
    nvs_get_str(handle, "pat_token", cfg->pat_token, &len);

    len = sizeof(cfg->app_id);
    nvs_get_str(handle, "app_id", cfg->app_id, &len);

    int32_t interval = 0;
    if (nvs_get_i32(handle, "poll_sec", &interval) == ESP_OK && interval >= 10) {
        cfg->poll_interval_sec = (int)interval;
    }

    nvs_close(handle);

    // 自动升级旧的失效测试 Token 与旧默认应用 ID
    bool need_save = false;
    if (strcmp(cfg->pat_token, "usr_9a4f6e1b7c3d2e5a8f01") == 0 || cfg->pat_token[0] == '\0') {
        strncpy(cfg->pat_token, EEVEE_DEFAULT_PAT_TOKEN, sizeof(cfg->pat_token) - 1);
        need_save = true;
    }
    if (strcmp(cfg->app_id, "app_env_monitor") == 0 || cfg->app_id[0] == '\0') {
        strncpy(cfg->app_id, EEVEE_DEFAULT_APP_ID, sizeof(cfg->app_id) - 1);
        need_save = true;
    }
    if (need_save) {
        eevee_config_save(cfg);
    }

    ESP_LOGI(TAG, "Loaded config: BaseURL=%s, AppID=%s, Interval=%ds",
             cfg->base_url, cfg->app_id, cfg->poll_interval_sec);
}

bool eevee_config_save(const eevee_config_t *cfg)
{
    if (!cfg) return false;
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return false;

    nvs_set_str(handle, "wifi_ssid", cfg->wifi_ssid);
    nvs_set_str(handle, "wifi_pass", cfg->wifi_pass);
    nvs_set_str(handle, "base_url", cfg->base_url);
    nvs_set_str(handle, "pat_token", cfg->pat_token);
    nvs_set_str(handle, "app_id", cfg->app_id);
    nvs_set_i32(handle, "poll_sec", cfg->poll_interval_sec);

    err = nvs_commit(handle);
    nvs_close(handle);
    return (err == ESP_OK);
}
