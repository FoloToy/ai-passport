#include "pomodoro_store.h"

#include <stddef.h>
#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define STORE_MAGIC 0x504F4D4FU
#define STORE_VERSION 1

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint8_t state;
    uint8_t selected_index;
    uint8_t reward_pending;
    uint8_t pomodoro_round;
    uint8_t pending_break_min;
    uint8_t muted;
    uint16_t reserved;
    uint32_t remaining_sec;
    uint32_t session_id;
    uint32_t completed_sessions;
    uint32_t completed_focus_min;
    uint32_t break_remaining_sec;
    uint32_t crc32;
} store_blob_t;

static const char *TAG = "pomo_store";
static QueueHandle_t s_queue;
static nvs_handle_t s_nvs;
static bool s_ready;
static volatile bool s_error;

static uint32_t crc32_bytes(const void *data, size_t len) {
    const uint8_t *bytes = data;
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < len; i++) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320U & (uint32_t)-(int32_t)(crc & 1));
        }
    }
    return ~crc;
}

static store_blob_t blob_from_model(const pomodoro_model_t *model) {
    store_blob_t blob = {
        .magic = STORE_MAGIC,
        .version = STORE_VERSION,
        .size = sizeof(store_blob_t),
        .state = (uint8_t)model->state,
        .selected_index = model->selected_index,
        .reward_pending = model->reward_pending,
        .pomodoro_round = model->pomodoro_round,
        .pending_break_min = model->pending_break_min,
        .muted = model->muted,
        .remaining_sec = model->remaining_sec,
        .session_id = model->session_id,
        .completed_sessions = model->completed_sessions,
        .completed_focus_min = model->completed_focus_min,
        .break_remaining_sec = model->break_remaining_sec,
    };
    blob.crc32 = crc32_bytes(&blob, offsetof(store_blob_t, crc32));
    return blob;
}

static bool model_from_blob(const store_blob_t *blob, pomodoro_model_t *model) {
    if (blob->magic != STORE_MAGIC || blob->version != STORE_VERSION ||
        blob->size != sizeof(*blob) ||
        blob->crc32 != crc32_bytes(blob, offsetof(store_blob_t, crc32))) {
        return false;
    }

    pomodoro_model_defaults(model);
    model->state = (pomodoro_state_t)blob->state;
    model->selected_index = blob->selected_index;
    model->reward_pending = blob->reward_pending;
    model->pomodoro_round = blob->pomodoro_round;
    model->pending_break_min = blob->pending_break_min;
    model->muted = blob->muted;
    model->remaining_sec = blob->remaining_sec;
    model->session_id = blob->session_id;
    model->completed_sessions = blob->completed_sessions;
    model->completed_focus_min = blob->completed_focus_min;
    model->break_remaining_sec = blob->break_remaining_sec;
    pomodoro_model_restore(model);
    return true;
}

static void save_task(void *arg) {
    (void)arg;
    store_blob_t blob;
    while (true) {
        if (xQueueReceive(s_queue, &blob, portMAX_DELAY) == pdTRUE) {
            esp_err_t err = nvs_set_blob(s_nvs, "state", &blob, sizeof(blob));
            if (err == ESP_OK) err = nvs_commit(s_nvs);
            if (err != ESP_OK) {
                s_error = true;
                ESP_LOGE(TAG, "NVS save failed: %s", esp_err_to_name(err));
            }
        }
    }
}

bool pomodoro_store_init(pomodoro_model_t *model) {
    if (!model) return false;
    if (s_ready) return true;

    pomodoro_model_defaults(model);
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs recovery: %s", esp_err_to_name(err));
        err = nvs_flash_erase();
        if (err == ESP_OK) err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(err));
        s_error = true;
        return false;
    }

    err = nvs_open("pomo", NVS_READWRITE, &s_nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        s_error = true;
        return false;
    }

    store_blob_t blob;
    size_t size = sizeof(blob);
    err = nvs_get_blob(s_nvs, "state", &blob, &size);
    if (err == ESP_OK && size == sizeof(blob)) {
        if (!model_from_blob(&blob, model)) {
            ESP_LOGW(TAG, "Stored state invalid; using defaults");
        } else {
            ESP_LOGI(TAG, "Restored %lu completed focus sessions",
                     (unsigned long)model->completed_sessions);
        }
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "NVS load failed: %s", esp_err_to_name(err));
    }

    s_queue = xQueueCreate(1, sizeof(store_blob_t));
    if (!s_queue || xTaskCreate(save_task, "pomo_save", 3072, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create persistence worker");
        s_error = true;
        return false;
    }
    s_ready = true;
    return true;
}

void pomodoro_store_request_save(const pomodoro_model_t *model) {
    if (!s_ready || !model) return;
    store_blob_t blob = blob_from_model(model);
    if (xQueueOverwrite(s_queue, &blob) != pdTRUE) s_error = true;
}

bool pomodoro_store_has_error(void) {
    return s_error;
}
