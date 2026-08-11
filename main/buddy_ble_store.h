#pragma once

#include <stdbool.h>

typedef struct {
    void *context;
    int (*erase_persistent)(void *context);
    int (*persistent_is_empty)(void *context, bool *empty);
    int (*reload_volatile)(void *context);
    int (*volatile_is_empty)(void *context, bool *empty);
} buddy_ble_store_ops_t;

int buddy_ble_store_clear_verified(const buddy_ble_store_ops_t *ops);
