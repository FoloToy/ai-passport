// main/passport_store.c —— "smpass" 命名空间的唯一读写模块。
#include "passport_store.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "smpass_store";
static const char *NVS_NS = "smpass";

esp_err_t passport_store_init(void)
{
    // 不自动擦除：分区里还有 FoloToy 出厂预配数据（红线见实现文档 7.2 节）。
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS 初始化失败: %s（未自动擦除分区）", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t open_ro(nvs_handle_t *out)
{
    return nvs_open(NVS_NS, NVS_READONLY, out);
}

static esp_err_t open_rw(nvs_handle_t *out)
{
    return nvs_open(NVS_NS, NVS_READWRITE, out);
}

esp_err_t passport_store_get_string(const char *key, char *buf, size_t len)
{
    nvs_handle_t h;
    esp_err_t err = open_ro(&h);
    if (err != ESP_OK) return err;
    size_t need = len;
    err = nvs_get_str(h, key, buf, &need);
    nvs_close(h);
    return err;
}

esp_err_t passport_store_set_string(const char *key, const char *val)
{
    nvs_handle_t h;
    esp_err_t err = open_rw(&h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, key, val);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t passport_store_get_i32(const char *key, int32_t *out)
{
    nvs_handle_t h;
    esp_err_t err = open_ro(&h);
    if (err != ESP_OK) return err;
    err = nvs_get_i32(h, key, out);
    nvs_close(h);
    return err;
}

esp_err_t passport_store_set_i32(const char *key, int32_t val)
{
    nvs_handle_t h;
    esp_err_t err = open_rw(&h);
    if (err != ESP_OK) return err;
    err = nvs_set_i32(h, key, val);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t passport_store_get_u8(const char *key, uint8_t *out)
{
    nvs_handle_t h;
    esp_err_t err = open_ro(&h);
    if (err != ESP_OK) return err;
    err = nvs_get_u8(h, key, out);
    nvs_close(h);
    return err;
}

esp_err_t passport_store_set_u8(const char *key, uint8_t val)
{
    nvs_handle_t h;
    esp_err_t err = open_rw(&h);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(h, key, val);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

bool passport_store_is_provisioned(void)
{
    char pid[32];
    uint8_t secret[PASSPORT_SECRET_LEN];
    bool ok = passport_store_get_string(PASSPORT_KEY_PID, pid, sizeof(pid)) == ESP_OK &&
              passport_store_get_secret(secret) == ESP_OK;
    memset(secret, 0, sizeof(secret));
    return ok;
}

esp_err_t passport_store_get_secret(uint8_t out[PASSPORT_SECRET_LEN])
{
    nvs_handle_t h;
    esp_err_t err = open_ro(&h);
    if (err != ESP_OK) return err;
    size_t need = PASSPORT_SECRET_LEN;
    err = nvs_get_blob(h, PASSPORT_KEY_SECRET, out, &need);
    nvs_close(h);
    if (err == ESP_OK && need != PASSPORT_SECRET_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }
    return err;
}

esp_err_t passport_store_provision(const char *pid,
                                   const uint8_t secret[PASSPORT_SECRET_LEN])
{
    nvs_handle_t h;
    esp_err_t err = open_rw(&h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, PASSPORT_KEY_PID, pid);
    if (err == ESP_OK) {
        err = nvs_set_blob(h, PASSPORT_KEY_SECRET, secret, PASSPORT_SECRET_LEN);
    }
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t passport_store_erase_all(void)
{
    nvs_handle_t h;
    esp_err_t err = open_rw(&h);
    if (err != ESP_OK) return err;
    err = nvs_erase_all(h);   // 只清 "smpass" 命名空间
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    ESP_LOGW(TAG, "已擦除 %s 命名空间（恢复出厂）", NVS_NS);
    return err;
}
