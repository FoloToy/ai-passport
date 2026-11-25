// main/wifi_store.c —— WiFi 凭据的 NVS 读写。
// namespace "wifi_cfg": ssid = 网络名(<=32 字节), pass = 密码(<=64 字节)。
// 写入后立即 commit:NVS 有掉电风险,配网这种「一次成功」的操作不能靠延迟写。
#include "wifi_store.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "wifi_store";

#define NS_NAME "wifi_cfg"
#define KEY_SSID "ssid"
#define KEY_PASS "pass"

#define SSID_CAP 33   // 802.11 SSID 上限 32 字节 + '\0'
#define PASS_CAP 65   // WPA2 口令上限 63 字节 + '\0'

// NVS 在 net_wifi 里初始化过一次;这里再调一次只是增加引用计数,
// 保证配网模块单独被调用时也不会因为「未初始化」失败。
static bool store_ready(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS 需要重建: %s", esp_err_to_name(err));
        if (nvs_flash_erase() != ESP_OK) return false;
        err = nvs_flash_init();
    }
    return err == ESP_OK;
}

bool wifi_store_load(char *ssid, size_t ssid_cap, char *pass, size_t pass_cap) {
    if (ssid == NULL || ssid_cap == 0) return false;
    ssid[0] = '\0';
    if (pass != NULL && pass_cap > 0) pass[0] = '\0';
    if (!store_ready()) return false;

    nvs_handle_t h;
    if (nvs_open(NS_NAME, NVS_READONLY, &h) != ESP_OK) return false;

    size_t len = ssid_cap;
    esp_err_t err = nvs_get_str(h, KEY_SSID, ssid, &len);
    if (err != ESP_OK || ssid[0] == '\0') {
        nvs_close(h);
        ssid[0] = '\0';
        return false;
    }
    if (pass != NULL && pass_cap > 0) {
        len = pass_cap;
        if (nvs_get_str(h, KEY_PASS, pass, &len) != ESP_OK) pass[0] = '\0';
    }
    nvs_close(h);
    return true;
}

bool wifi_store_save(const char *ssid, const char *pass) {
    if (ssid == NULL || ssid[0] == '\0') return false;
    if (strlen(ssid) >= SSID_CAP) {
        ESP_LOGE(TAG, "SSID 过长(%u 字节,上限 32)", (unsigned)strlen(ssid));
        return false;
    }
    if (pass != NULL && strlen(pass) >= PASS_CAP) {
        ESP_LOGE(TAG, "密码过长(%u 字节,上限 63)", (unsigned)strlen(pass));
        return false;
    }
    if (!store_ready()) return false;

    nvs_handle_t h;
    esp_err_t err = nvs_open(NS_NAME, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "打开 NVS 失败: %s", esp_err_to_name(err));
        return false;
    }
    err = nvs_set_str(h, KEY_SSID, ssid);
    if (err == ESP_OK) err = nvs_set_str(h, KEY_PASS, (pass != NULL) ? pass : "");
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "写入凭据失败: %s", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "凭据已保存: \"%s\"(%u 字节密码)", ssid, (unsigned)((pass != NULL) ? strlen(pass) : 0));
    return true;
}

bool wifi_store_clear(void) {
    if (!store_ready()) return false;
    nvs_handle_t h;
    if (nvs_open(NS_NAME, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = nvs_erase_all(h);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}
