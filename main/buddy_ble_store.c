#include "buddy_ble_store.h"

#include <stddef.h>

#define BUDDY_BLE_STORE_INVALID_ARG (-1)
#define BUDDY_BLE_STORE_NOT_EMPTY (-2)

int buddy_ble_store_clear_verified(const buddy_ble_store_ops_t *ops)
{
    bool empty;
    int rc;

    if (ops == NULL || ops->erase_persistent == NULL || ops->persistent_is_empty == NULL ||
        ops->reload_volatile == NULL || ops->volatile_is_empty == NULL) {
        return BUDDY_BLE_STORE_INVALID_ARG;
    }

    if (ops->remove_controller_peers != NULL) {
        rc = ops->remove_controller_peers(ops->context);
        if (rc != 0) {
            return rc;
        }
    }

    rc = ops->erase_persistent(ops->context);
    if (rc != 0) {
        return rc;
    }
    rc = ops->persistent_is_empty(ops->context, &empty);
    if (rc != 0) {
        return rc;
    }
    if (!empty) {
        return BUDDY_BLE_STORE_NOT_EMPTY;
    }

    rc = ops->reload_volatile(ops->context);
    if (rc != 0) {
        return rc;
    }
    if (ops->restore_local_identity != NULL) {
        rc = ops->restore_local_identity(ops->context);
        if (rc != 0) {
            return rc;
        }
    }
    rc = ops->persistent_is_clean != NULL
             ? ops->persistent_is_clean(ops->context, &empty)
             : ops->persistent_is_empty(ops->context, &empty);
    if (rc != 0) {
        return rc;
    }
    if (!empty) {
        return BUDDY_BLE_STORE_NOT_EMPTY;
    }
    rc = ops->volatile_is_empty(ops->context, &empty);
    if (rc != 0) {
        return rc;
    }
    if (!empty) {
        return BUDDY_BLE_STORE_NOT_EMPTY;
    }
    if (ops->controller_is_clean != NULL) {
        rc = ops->controller_is_clean(ops->context, &empty);
        if (rc != 0) {
            return rc;
        }
        if (!empty) {
            return BUDDY_BLE_STORE_NOT_EMPTY;
        }
    }
    return 0;
}
