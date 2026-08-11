#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#include "buddy_types.h"

esp_err_t buddy_settings_init(void);
esp_err_t buddy_settings_load(buddy_settings_snapshot_t *snapshot);
esp_err_t buddy_settings_set_name(const char *name);
esp_err_t buddy_settings_set_owner(const char *owner);
esp_err_t buddy_settings_set_name_committed(const char *name);
esp_err_t buddy_settings_set_owner_committed(const char *owner);
esp_err_t buddy_settings_set_ble_enabled(bool enabled);
esp_err_t buddy_settings_set_highest_celebrated_level(uint64_t level);
void buddy_settings_record_permission(buddy_permission_decision_t decision);
esp_err_t buddy_settings_flush(bool force);
esp_err_t buddy_settings_factory_reset(void);

#ifdef BUDDY_SETTINGS_TESTING
typedef struct {
    esp_err_t (*get_str)(void *context, const char *key, char *value, size_t *length);
    esp_err_t (*set_str)(void *context, const char *key, const char *value);
    esp_err_t (*get_u8)(void *context, const char *key, uint8_t *value);
    esp_err_t (*set_u8)(void *context, const char *key, uint8_t value);
    esp_err_t (*get_u64)(void *context, const char *key, uint64_t *value);
    esp_err_t (*set_u64)(void *context, const char *key, uint64_t value);
    esp_err_t (*erase_all)(void *context);
    esp_err_t (*commit)(void *context);
} buddy_settings_backend_t;

void buddy_settings_test_set_backend(const buddy_settings_backend_t *backend, void *context);
void buddy_settings_test_set_time_ms(uint64_t now_ms);
#endif
