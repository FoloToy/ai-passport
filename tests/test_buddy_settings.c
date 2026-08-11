#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "buddy_settings.h"

typedef struct {
    char name[BUDDY_NAME_MAX];
    char owner[BUDDY_OWNER_MAX];
    uint8_t ble;
    uint64_t approvals;
    uint64_t denials;
    uint64_t level;
    bool has_name;
    bool has_owner;
    bool has_ble;
    bool has_approvals;
    bool has_denials;
    bool has_level;
    unsigned commit_count;
    unsigned erase_count;
} fake_storage_t;

static esp_err_t fake_get_str(void *context, const char *key, char *value, size_t *length)
{
    fake_storage_t *storage = context;
    const char *stored = NULL;
    bool present = false;
    size_t needed;

    if (strcmp(key, "name") == 0) {
        stored = storage->name;
        present = storage->has_name;
    } else if (strcmp(key, "owner") == 0) {
        stored = storage->owner;
        present = storage->has_owner;
    }
    if (!present) {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    needed = strlen(stored) + 1;
    if (*length < needed) {
        *length = needed;
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(value, stored, needed);
    *length = needed;
    return ESP_OK;
}

static esp_err_t fake_set_str(void *context, const char *key, const char *value)
{
    fake_storage_t *storage = context;

    if (strcmp(key, "name") == 0) {
        snprintf(storage->name, sizeof(storage->name), "%s", value);
        storage->has_name = true;
    } else if (strcmp(key, "owner") == 0) {
        snprintf(storage->owner, sizeof(storage->owner), "%s", value);
        storage->has_owner = true;
    } else {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static esp_err_t fake_get_u8(void *context, const char *key, uint8_t *value)
{
    fake_storage_t *storage = context;

    if (strcmp(key, "ble") != 0 || !storage->has_ble) {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    *value = storage->ble;
    return ESP_OK;
}

static esp_err_t fake_set_u8(void *context, const char *key, uint8_t value)
{
    fake_storage_t *storage = context;

    if (strcmp(key, "ble") != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    storage->ble = value;
    storage->has_ble = true;
    return ESP_OK;
}

static esp_err_t fake_get_u64(void *context, const char *key, uint64_t *value)
{
    fake_storage_t *storage = context;
    bool present = false;

    if (strcmp(key, "approve") == 0) {
        *value = storage->approvals;
        present = storage->has_approvals;
    } else if (strcmp(key, "deny") == 0) {
        *value = storage->denials;
        present = storage->has_denials;
    } else if (strcmp(key, "level") == 0) {
        *value = storage->level;
        present = storage->has_level;
    }
    return present ? ESP_OK : ESP_ERR_NVS_NOT_FOUND;
}

static esp_err_t fake_set_u64(void *context, const char *key, uint64_t value)
{
    fake_storage_t *storage = context;

    if (strcmp(key, "approve") == 0) {
        storage->approvals = value;
        storage->has_approvals = true;
    } else if (strcmp(key, "deny") == 0) {
        storage->denials = value;
        storage->has_denials = true;
    } else if (strcmp(key, "level") == 0) {
        storage->level = value;
        storage->has_level = true;
    } else {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static esp_err_t fake_erase_all(void *context)
{
    fake_storage_t *storage = context;

    memset(storage->name, 0, sizeof(storage->name));
    memset(storage->owner, 0, sizeof(storage->owner));
    storage->has_name = false;
    storage->has_owner = false;
    storage->has_ble = false;
    storage->has_approvals = false;
    storage->has_denials = false;
    storage->has_level = false;
    ++storage->erase_count;
    return ESP_OK;
}

static esp_err_t fake_commit(void *context)
{
    fake_storage_t *storage = context;

    ++storage->commit_count;
    return ESP_OK;
}

static const buddy_settings_backend_t fake_backend = {
    .get_str = fake_get_str,
    .set_str = fake_set_str,
    .get_u8 = fake_get_u8,
    .set_u8 = fake_set_u8,
    .get_u64 = fake_get_u64,
    .set_u64 = fake_set_u64,
    .erase_all = fake_erase_all,
    .commit = fake_commit,
};

static void test_setup(fake_storage_t *storage)
{
    memset(storage, 0, sizeof(*storage));
    buddy_settings_test_set_backend(&fake_backend, storage);
    buddy_settings_test_set_time_ms(0);
    assert(buddy_settings_init() == ESP_OK);
}

static void test_rapid_permissions_only_commit_on_forced_flush(void)
{
    fake_storage_t storage;
    buddy_settings_snapshot_t snapshot;
    int index;

    test_setup(&storage);
    for (index = 0; index < 5; ++index) {
        buddy_settings_record_permission(BUDDY_PERMISSION_ONCE);
    }
    assert(buddy_settings_load(&snapshot) == ESP_OK);
    assert(snapshot.approval_count == 5);
    assert(storage.commit_count == 0);
    assert(buddy_settings_flush(true) == ESP_OK);
    assert(storage.commit_count == 1);
    assert(storage.approvals == 5);
}

static void test_regular_flush_is_limited_to_once_per_minute(void)
{
    fake_storage_t storage;

    test_setup(&storage);
    buddy_settings_record_permission(BUDDY_PERMISSION_DENY);
    assert(buddy_settings_flush(false) == ESP_OK);
    assert(storage.commit_count == 1);
    buddy_settings_record_permission(BUDDY_PERMISSION_DENY);
    buddy_settings_test_set_time_ms(59999);
    assert(buddy_settings_flush(false) == ESP_OK);
    assert(storage.commit_count == 1);
    buddy_settings_test_set_time_ms(60000);
    assert(buddy_settings_flush(false) == ESP_OK);
    assert(storage.commit_count == 2);
    assert(storage.denials == 2);
}

static void test_name_validation_and_defaults(void)
{
    fake_storage_t storage;
    buddy_settings_snapshot_t snapshot;
    char too_long[BUDDY_NAME_MAX + 1];
    const char invalid_utf8[] = {'C', (char)0xc0, (char)0x80, '\0'};

    test_setup(&storage);
    assert(buddy_settings_load(&snapshot) == ESP_OK);
    assert(snapshot.name[0] == '\0');
    assert(snapshot.owner[0] == '\0');
    assert(snapshot.ble_enabled);

    memset(too_long, 'a', sizeof(too_long));
    too_long[sizeof(too_long) - 1] = '\0';
    assert(buddy_settings_set_name("") == ESP_ERR_INVALID_ARG);
    assert(buddy_settings_set_name(invalid_utf8) == ESP_ERR_INVALID_ARG);
    assert(buddy_settings_set_name(too_long) == ESP_ERR_INVALID_ARG);
    assert(buddy_settings_set_name("Claude-Buddy") == ESP_OK);
    assert(buddy_settings_load(&snapshot) == ESP_OK);
    assert(strcmp(snapshot.name, "Claude-Buddy") == 0);
}

static void test_settings_round_trip_through_storage(void)
{
    fake_storage_t storage;
    buddy_settings_snapshot_t snapshot;

    test_setup(&storage);
    assert(buddy_settings_set_name("Claude-C3") == ESP_OK);
    assert(buddy_settings_set_owner("Ada") == ESP_OK);
    assert(buddy_settings_set_ble_enabled(false) == ESP_OK);
    assert(buddy_settings_set_highest_celebrated_level(7) == ESP_OK);
    assert(buddy_settings_flush(true) == ESP_OK);

    buddy_settings_test_set_backend(&fake_backend, &storage);
    assert(buddy_settings_init() == ESP_OK);
    assert(buddy_settings_load(&snapshot) == ESP_OK);
    assert(strcmp(snapshot.name, "Claude-C3") == 0);
    assert(strcmp(snapshot.owner, "Ada") == 0);
    assert(!snapshot.ble_enabled);
    assert(snapshot.highest_celebrated_level == 7);
}

static void test_factory_reset_only_erases_buddy_backend(void)
{
    fake_storage_t storage;
    buddy_settings_snapshot_t snapshot;

    test_setup(&storage);
    assert(buddy_settings_set_owner("Ada") == ESP_OK);
    buddy_settings_record_permission(BUDDY_PERMISSION_ALWAYS);
    assert(buddy_settings_factory_reset() == ESP_OK);
    assert(storage.erase_count == 1);
    assert(storage.commit_count == 1);
    assert(buddy_settings_load(&snapshot) == ESP_OK);
    assert(snapshot.owner[0] == '\0');
    assert(snapshot.approval_count == 0);
    assert(snapshot.ble_enabled);
}

int main(void)
{
    test_rapid_permissions_only_commit_on_forced_flush();
    test_regular_flush_is_limited_to_once_per_minute();
    test_name_validation_and_defaults();
    test_settings_round_trip_through_storage();
    test_factory_reset_only_erases_buddy_backend();
    puts("buddy settings tests passed");
    return 0;
}
