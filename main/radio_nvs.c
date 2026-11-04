#include "radio_nvs.h"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <string.h>

static const char *TAG = "radio_nvs";
static const char *NS = "gradio";

esp_err_t radio_nvs_load(radio_settings_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->volume = 60;
    strncpy(out->station_id, "cnr1", sizeof(out->station_id) - 1);

    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    size_t len = sizeof(out->ssid);
    if (nvs_get_str(h, "ssid", out->ssid, &len) == ESP_OK && out->ssid[0]) {
        len = sizeof(out->password);
        if (nvs_get_str(h, "pass", out->password, &len) != ESP_OK) {
            out->password[0] = '\0';
        }
        out->has_wifi = true;
    }

    len = sizeof(out->station_id);
    (void)nvs_get_str(h, "station", out->station_id, &len);

    uint8_t vol = out->volume;
    if (nvs_get_u8(h, "vol", &vol) == ESP_OK) {
        out->volume = vol > 100 ? 100 : vol;
    }
    nvs_close(h);
    return ESP_OK;
}

esp_err_t radio_nvs_save(const radio_settings_t *in)
{
    if (!in) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "open: %s", esp_err_to_name(err));
        return err;
    }
    if (in->has_wifi && in->ssid[0]) {
        err = nvs_set_str(h, "ssid", in->ssid);
        if (err == ESP_OK) {
            err = nvs_set_str(h, "pass", in->password);
        }
    }
    if (err == ESP_OK && in->station_id[0]) {
        err = nvs_set_str(h, "station", in->station_id);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(h, "vol", in->volume > 100 ? 100 : in->volume);
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

esp_err_t radio_nvs_clear_wifi(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    (void)nvs_erase_key(h, "ssid");
    (void)nvs_erase_key(h, "pass");
    err = nvs_commit(h);
    nvs_close(h);
    return err;
}
