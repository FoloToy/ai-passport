#include "tamagezi_store.h"

#include <stddef.h>
#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define STORE_MAGIC 0x544D5A53U

typedef struct {
    uint32_t magic;
    uint32_t sequence;
    uint32_t model_size;
    tmz_model_t model;
    uint32_t crc;
} store_record_t;

static const char *TAG = "tmz_store";
static const char *NAMESPACE = "tamagezi";
static QueueHandle_t s_save_queue;
static uint32_t s_sequence;

static uint32_t crc32(const void *data, size_t length)
{
    const uint8_t *bytes = data;
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < length; i++) {
        crc ^= bytes[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320U & (uint32_t)-(int32_t)(crc & 1U));
        }
    }
    return ~crc;
}

static bool record_valid(const store_record_t *record, size_t length)
{
    if (length != sizeof(*record) || record->magic != STORE_MAGIC ||
        record->model_size != sizeof(record->model)) {
        return false;
    }
    return record->crc == crc32(record, offsetof(store_record_t, crc)) &&
           record->model.version == TMZ_MODEL_VERSION;
}

static bool read_slot(nvs_handle_t nvs, const char *key, store_record_t *record)
{
    size_t length = sizeof(*record);
    esp_err_t err = nvs_get_blob(nvs, key, record, &length);
    return err == ESP_OK && record_valid(record, length);
}

static void save_task(void *context)
{
    (void)context;
    tmz_model_t model;
    while (true) {
        if (xQueueReceive(s_save_queue, &model, portMAX_DELAY) != pdTRUE) continue;
        store_record_t record = {
            .magic = STORE_MAGIC,
            .sequence = ++s_sequence,
            .model_size = sizeof(model),
            .model = model,
        };
        record.crc = crc32(&record, offsetof(store_record_t, crc));
        nvs_handle_t nvs;
        esp_err_t err = nvs_open(NAMESPACE, NVS_READWRITE, &nvs);
        if (err == ESP_OK) {
            const char *key = (record.sequence & 1U) ? "slot_a" : "slot_b";
            err = nvs_set_blob(nvs, key, &record, sizeof(record));
            if (err == ESP_OK) err = nvs_commit(nvs);
            nvs_close(nvs);
        }
        if (err != ESP_OK) ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(err));
    }
}

bool tmz_store_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs recovery");
        if (nvs_flash_erase() != ESP_OK) return false;
        err = nvs_flash_init();
    }
    if (err != ESP_OK) return false;
    if (s_save_queue) return true;
    s_save_queue = xQueueCreate(1, sizeof(tmz_model_t));
    if (!s_save_queue) return false;
    if (xTaskCreate(save_task, "tmz_save", 3072, NULL, 3, NULL) != pdPASS) {
        vQueueDelete(s_save_queue);
        s_save_queue = NULL;
        return false;
    }
    return true;
}

bool tmz_store_load(tmz_model_t *model)
{
    if (!model) return false;
    nvs_handle_t nvs;
    if (nvs_open(NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) return false;
    store_record_t a;
    store_record_t b;
    bool a_ok = read_slot(nvs, "slot_a", &a);
    bool b_ok = read_slot(nvs, "slot_b", &b);
    nvs_close(nvs);
    if (!a_ok && !b_ok) return false;
    const store_record_t *chosen = !a_ok ? &b : !b_ok ? &a :
                                     (int32_t)(a.sequence - b.sequence) > 0 ? &a : &b;
    *model = chosen->model;
    s_sequence = chosen->sequence;
    return tmz_model_sanitize(model);
}

bool tmz_store_save_async(const tmz_model_t *model)
{
    return s_save_queue && model &&
           xQueueOverwrite(s_save_queue, model) == pdPASS;
}
