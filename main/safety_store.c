#include "safety_store.h"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#define SAFETY_NAMESPACE "trae_cfg"
#define SAFETY_PROFILE_KEY "safe_card"

static const char *TAG = "safety_store";
static bool s_ready;

bool safety_store_init(void)
{
    if (s_ready) return true;
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        // Never erase automatically: this partition can contain the user's
        // safety profile, and a failed migration should remain recoverable.
        ESP_LOGE(TAG, "NVS init failed without erasing user data: %s",
                 esp_err_to_name(err));
        return false;
    }
    s_ready = true;
    return true;
}

bool safety_store_load(safety_profile_t *profile)
{
    if (!profile) return false;
    safety_profile_defaults(profile);
    if (!safety_store_init()) return false;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(SAFETY_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) return false;
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open for read failed: %s", esp_err_to_name(err));
        return false;
    }

    safety_profile_t stored;
    size_t length = sizeof(stored);
    err = nvs_get_blob(handle, SAFETY_PROFILE_KEY, &stored, &length);
    nvs_close(handle);
    if (err != ESP_OK || length != sizeof(stored) ||
        !safety_profile_is_valid(&stored)) {
        if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Stored profile is unavailable or invalid");
        }
        return false;
    }

    *profile = stored;
    return profile->configured != 0;
}

bool safety_store_save(safety_profile_t *profile)
{
    if (!profile || !safety_store_init()) return false;
    safety_profile_seal(profile);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(SAFETY_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return false;
    err = nvs_set_blob(handle, SAFETY_PROFILE_KEY, profile, sizeof(*profile));
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Profile save failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}
