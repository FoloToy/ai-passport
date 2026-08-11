#pragma once

#include <stdbool.h>

typedef struct {
    void *context;
    int (*remove_controller_peers)(void *context);
    int (*erase_persistent)(void *context);
    int (*persistent_is_empty)(void *context, bool *empty);
    int (*reload_volatile)(void *context);
    int (*restore_local_identity)(void *context);
    int (*persistent_is_clean)(void *context, bool *clean);
    int (*volatile_is_empty)(void *context, bool *empty);
    int (*controller_is_clean)(void *context, bool *clean);
} buddy_ble_store_ops_t;

int buddy_ble_store_clear_verified(const buddy_ble_store_ops_t *ops);
