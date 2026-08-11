#include "buddy_settings.h"

#include <limits.h>
#include <string.h>

#ifdef ESP_PLATFORM
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#endif

#define BUDDY_SETTINGS_NAMESPACE "buddy"
#define BUDDY_SETTINGS_FLUSH_INTERVAL_MS 60000ULL

#define BUDDY_SETTINGS_DIRTY_NAME (1U << 0)
#define BUDDY_SETTINGS_DIRTY_OWNER (1U << 1)
#define BUDDY_SETTINGS_DIRTY_BLE (1U << 2)
#define BUDDY_SETTINGS_DIRTY_APPROVE (1U << 3)
#define BUDDY_SETTINGS_DIRTY_DENY (1U << 4)
#define BUDDY_SETTINGS_DIRTY_LEVEL (1U << 5)

typedef struct {
    esp_err_t (*get_str)(void *context, const char *key, char *value, size_t *length);
    esp_err_t (*set_str)(void *context, const char *key, const char *value);
    esp_err_t (*get_u8)(void *context, const char *key, uint8_t *value);
    esp_err_t (*set_u8)(void *context, const char *key, uint8_t value);
    esp_err_t (*get_u64)(void *context, const char *key, uint64_t *value);
    esp_err_t (*set_u64)(void *context, const char *key, uint64_t value);
    esp_err_t (*erase_all)(void *context);
    esp_err_t (*commit)(void *context);
} buddy_settings_backend_internal_t;

static buddy_settings_snapshot_t s_settings;
static const buddy_settings_backend_internal_t *s_backend;
static void *s_backend_context;
static uint32_t s_dirty;
static uint64_t s_last_flush_ms;
static bool s_initialized;
static bool s_loaded;
static bool s_has_flushed;

#ifdef BUDDY_SETTINGS_TESTING
static uint64_t s_test_now_ms;
static buddy_settings_backend_internal_t s_test_backend;
#endif

#ifdef ESP_PLATFORM
static nvs_handle_t s_nvs_handle;

static esp_err_t buddy_nvs_get_str(void *context, const char *key, char *value, size_t *length)
{
    (void)context;
    return nvs_get_str(s_nvs_handle, key, value, length);
}

static esp_err_t buddy_nvs_set_str(void *context, const char *key, const char *value)
{
    (void)context;
    return nvs_set_str(s_nvs_handle, key, value);
}

static esp_err_t buddy_nvs_get_u8(void *context, const char *key, uint8_t *value)
{
    (void)context;
    return nvs_get_u8(s_nvs_handle, key, value);
}

static esp_err_t buddy_nvs_set_u8(void *context, const char *key, uint8_t value)
{
    (void)context;
    return nvs_set_u8(s_nvs_handle, key, value);
}

static esp_err_t buddy_nvs_get_u64(void *context, const char *key, uint64_t *value)
{
    (void)context;
    return nvs_get_u64(s_nvs_handle, key, value);
}

static esp_err_t buddy_nvs_set_u64(void *context, const char *key, uint64_t value)
{
    (void)context;
    return nvs_set_u64(s_nvs_handle, key, value);
}

static esp_err_t buddy_nvs_erase_all(void *context)
{
    (void)context;
    return nvs_erase_all(s_nvs_handle);
}

static esp_err_t buddy_nvs_commit(void *context)
{
    (void)context;
    return nvs_commit(s_nvs_handle);
}

static const buddy_settings_backend_internal_t s_nvs_backend = {
    .get_str = buddy_nvs_get_str,
    .set_str = buddy_nvs_set_str,
    .get_u8 = buddy_nvs_get_u8,
    .set_u8 = buddy_nvs_set_u8,
    .get_u64 = buddy_nvs_get_u64,
    .set_u64 = buddy_nvs_set_u64,
    .erase_all = buddy_nvs_erase_all,
    .commit = buddy_nvs_commit,
};
#endif

static void buddy_settings_defaults(void)
{
    memset(&s_settings, 0, sizeof(s_settings));
    s_settings.ble_enabled = true;
}

static uint64_t buddy_settings_now_ms(void)
{
#ifdef BUDDY_SETTINGS_TESTING
    return s_test_now_ms;
#elif defined(ESP_PLATFORM)
    return (uint64_t)esp_timer_get_time() / 1000ULL;
#else
    return 0;
#endif
}

static bool buddy_utf8_is_valid(const char *value, size_t length)
{
    const unsigned char *cursor = (const unsigned char *)value;
    const unsigned char *end = cursor + length;

    while (cursor < end) {
        unsigned char first = *cursor++;
        unsigned char second;
        unsigned char third;

        if (first <= 0x7fU) {
            continue;
        }
        if (first >= 0xc2U && first <= 0xdfU) {
            if ((size_t)(end - cursor) < 1U) {
                return false;
            }
            second = *cursor++;
            if ((second & 0xc0U) != 0x80U) {
                return false;
            }
            continue;
        }
        if (first >= 0xe0U && first <= 0xefU) {
            if ((size_t)(end - cursor) < 2U) {
                return false;
            }
            second = *cursor++;
            third = *cursor++;
            if ((second & 0xc0U) != 0x80U || (third & 0xc0U) != 0x80U ||
                (first == 0xe0U && second < 0xa0U) ||
                (first == 0xedU && second >= 0xa0U)) {
                return false;
            }
            continue;
        }
        if (first >= 0xf0U && first <= 0xf4U) {
            if ((size_t)(end - cursor) < 3U) {
                return false;
            }
            second = *cursor++;
            third = *cursor++;
            if ((second & 0xc0U) != 0x80U || (third & 0xc0U) != 0x80U ||
                (*cursor & 0xc0U) != 0x80U ||
                (first == 0xf0U && second < 0x90U) ||
                (first == 0xf4U && second >= 0x90U)) {
                return false;
            }
            ++cursor;
            continue;
        }
        return false;
    }
    return true;
}

static bool buddy_value_is_valid(const char *value, size_t capacity, bool required)
{
    size_t length;

    if (value == NULL) {
        return false;
    }
    for (length = 0; length < capacity; ++length) {
        if (value[length] == '\0') {
            return (!required || length > 0) && buddy_utf8_is_valid(value, length);
        }
    }
    return false;
}

static esp_err_t buddy_settings_ensure_loaded(void)
{
    buddy_settings_snapshot_t snapshot;

    return s_loaded ? ESP_OK : buddy_settings_load(&snapshot);
}

static esp_err_t buddy_settings_store_dirty(void)
{
    esp_err_t err;

    if ((s_dirty & BUDDY_SETTINGS_DIRTY_NAME) != 0U) {
        err = s_backend->set_str(s_backend_context, "name", s_settings.name);
        if (err != ESP_OK) return err;
    }
    if ((s_dirty & BUDDY_SETTINGS_DIRTY_OWNER) != 0U) {
        err = s_backend->set_str(s_backend_context, "owner", s_settings.owner);
        if (err != ESP_OK) return err;
    }
    if ((s_dirty & BUDDY_SETTINGS_DIRTY_BLE) != 0U) {
        err = s_backend->set_u8(s_backend_context, "ble", s_settings.ble_enabled ? 1U : 0U);
        if (err != ESP_OK) return err;
    }
    if ((s_dirty & BUDDY_SETTINGS_DIRTY_APPROVE) != 0U) {
        err = s_backend->set_u64(s_backend_context, "approve", s_settings.approval_count);
        if (err != ESP_OK) return err;
    }
    if ((s_dirty & BUDDY_SETTINGS_DIRTY_DENY) != 0U) {
        err = s_backend->set_u64(s_backend_context, "deny", s_settings.denial_count);
        if (err != ESP_OK) return err;
    }
    if ((s_dirty & BUDDY_SETTINGS_DIRTY_LEVEL) != 0U) {
        err = s_backend->set_u64(s_backend_context, "level",
                                 s_settings.highest_celebrated_level);
        if (err != ESP_OK) return err;
    }
    err = s_backend->commit(s_backend_context);
    if (err == ESP_OK) {
        s_dirty = 0;
    }
    return err;
}

esp_err_t buddy_settings_init(void)
{
    buddy_settings_defaults();
    s_dirty = 0;
    s_last_flush_ms = 0;
    s_loaded = false;
    s_has_flushed = false;

#ifdef BUDDY_SETTINGS_TESTING
    if (s_backend == NULL) {
        s_initialized = false;
        return ESP_ERR_INVALID_STATE;
    }
#elif defined(ESP_PLATFORM)
    esp_err_t err = nvs_flash_init();

    if (err != ESP_OK) {
        s_initialized = false;
        return err;
    }
    err = nvs_open(BUDDY_SETTINGS_NAMESPACE, NVS_READWRITE, &s_nvs_handle);

    if (err != ESP_OK) {
        s_initialized = false;
        return err;
    }
    s_backend = &s_nvs_backend;
    s_backend_context = NULL;
#else
    s_initialized = false;
    return ESP_ERR_INVALID_STATE;
#endif

    s_initialized = true;
    return ESP_OK;
}

esp_err_t buddy_settings_load(buddy_settings_snapshot_t *snapshot)
{
    esp_err_t err;
    size_t length;
    uint8_t ble;
    uint64_t value;

    if (!s_initialized || snapshot == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_loaded) {
        length = sizeof(s_settings.name);
        err = s_backend->get_str(s_backend_context, "name", s_settings.name, &length);
        if (err != ESP_OK || !buddy_value_is_valid(s_settings.name, sizeof(s_settings.name), false)) {
            s_settings.name[0] = '\0';
        }
        length = sizeof(s_settings.owner);
        err = s_backend->get_str(s_backend_context, "owner", s_settings.owner, &length);
        if (err != ESP_OK || !buddy_value_is_valid(s_settings.owner, sizeof(s_settings.owner), false)) {
            s_settings.owner[0] = '\0';
        }
        err = s_backend->get_u8(s_backend_context, "ble", &ble);
        s_settings.ble_enabled = err == ESP_OK && ble <= 1U ? ble != 0U : true;
        err = s_backend->get_u64(s_backend_context, "approve", &value);
        s_settings.approval_count = err == ESP_OK ? value : 0;
        err = s_backend->get_u64(s_backend_context, "deny", &value);
        s_settings.denial_count = err == ESP_OK ? value : 0;
        err = s_backend->get_u64(s_backend_context, "level", &value);
        s_settings.highest_celebrated_level = err == ESP_OK ? value : 0;
        s_loaded = true;
    }
    *snapshot = s_settings;
    return ESP_OK;
}

esp_err_t buddy_settings_set_name(const char *name)
{
    if (!s_initialized || !buddy_value_is_valid(name, sizeof(s_settings.name), true)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (buddy_settings_ensure_loaded() != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    memcpy(s_settings.name, name, strlen(name) + 1U);
    s_dirty |= BUDDY_SETTINGS_DIRTY_NAME;
    return ESP_OK;
}

esp_err_t buddy_settings_set_owner(const char *owner)
{
    if (!s_initialized || !buddy_value_is_valid(owner, sizeof(s_settings.owner), false)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (buddy_settings_ensure_loaded() != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    memcpy(s_settings.owner, owner, strlen(owner) + 1U);
    s_dirty |= BUDDY_SETTINGS_DIRTY_OWNER;
    return ESP_OK;
}

esp_err_t buddy_settings_set_ble_enabled(bool enabled)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (buddy_settings_ensure_loaded() != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    s_settings.ble_enabled = enabled;
    s_dirty |= BUDDY_SETTINGS_DIRTY_BLE;
    return ESP_OK;
}

esp_err_t buddy_settings_set_highest_celebrated_level(uint64_t level)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (buddy_settings_ensure_loaded() != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    s_settings.highest_celebrated_level = level;
    s_dirty |= BUDDY_SETTINGS_DIRTY_LEVEL;
    return ESP_OK;
}

void buddy_settings_record_permission(buddy_permission_decision_t decision)
{
    if (!s_initialized) {
        return;
    }
    if (buddy_settings_ensure_loaded() != ESP_OK) {
        return;
    }
    if (decision == BUDDY_PERMISSION_ONCE || decision == BUDDY_PERMISSION_ALWAYS) {
        if (s_settings.approval_count < UINT64_MAX) {
            ++s_settings.approval_count;
        }
        s_dirty |= BUDDY_SETTINGS_DIRTY_APPROVE;
    } else if (decision == BUDDY_PERMISSION_DENY) {
        if (s_settings.denial_count < UINT64_MAX) {
            ++s_settings.denial_count;
        }
        s_dirty |= BUDDY_SETTINGS_DIRTY_DENY;
    }
}

esp_err_t buddy_settings_flush(bool force)
{
    uint64_t now_ms;
    esp_err_t err;

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_dirty == 0U) {
        return ESP_OK;
    }
    now_ms = buddy_settings_now_ms();
    if (!force && s_has_flushed && now_ms - s_last_flush_ms < BUDDY_SETTINGS_FLUSH_INTERVAL_MS) {
        return ESP_OK;
    }
    err = buddy_settings_store_dirty();
    if (err == ESP_OK) {
        s_last_flush_ms = now_ms;
        s_has_flushed = true;
    }
    return err;
}

esp_err_t buddy_settings_factory_reset(void)
{
    esp_err_t err;

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    err = s_backend->erase_all(s_backend_context);
    if (err != ESP_OK) {
        return err;
    }
    err = s_backend->commit(s_backend_context);
    if (err == ESP_OK) {
        buddy_settings_defaults();
        s_dirty = 0;
        s_has_flushed = false;
        s_loaded = true;
    }
    return err;
}

#ifdef BUDDY_SETTINGS_TESTING
void buddy_settings_test_set_backend(const buddy_settings_backend_t *backend, void *context)
{
    if (backend == NULL) {
        s_backend = NULL;
        s_backend_context = NULL;
        return;
    }
    s_test_backend.get_str = backend->get_str;
    s_test_backend.set_str = backend->set_str;
    s_test_backend.get_u8 = backend->get_u8;
    s_test_backend.set_u8 = backend->set_u8;
    s_test_backend.get_u64 = backend->get_u64;
    s_test_backend.set_u64 = backend->set_u64;
    s_test_backend.erase_all = backend->erase_all;
    s_test_backend.commit = backend->commit;
    s_backend = &s_test_backend;
    s_backend_context = context;
}

void buddy_settings_test_set_time_ms(uint64_t now_ms)
{
    s_test_now_ms = now_ms;
}
#endif
