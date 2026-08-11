#include "buddy_ble_lifecycle.h"

#include <stddef.h>

#define BUDDY_BLE_LIFECYCLE_INVALID_ARG (-1)

int buddy_ble_lifecycle_init(const buddy_ble_lifecycle_ops_t *ops, bool *retry_safe)
{
    bool host_initialized = false;
    bool events_initialized = false;
    bool bond_callout_initialized = false;
    bool adv_callout_initialized = false;
    int rc;

    if (ops == NULL || retry_safe == NULL || ops->host_init == NULL ||
        ops->events_init == NULL || ops->bond_callout_init == NULL ||
        ops->adv_callout_init == NULL || ops->gatt_init == NULL ||
        ops->adv_callout_deinit == NULL || ops->bond_callout_deinit == NULL ||
        ops->events_deinit == NULL || ops->host_deinit == NULL || ops->clear == NULL) {
        return BUDDY_BLE_LIFECYCLE_INVALID_ARG;
    }
    *retry_safe = true;

    rc = ops->host_init(ops->context);
    if (rc != 0) {
        *retry_safe = false;
        return rc;
    }
    host_initialized = true;

    rc = ops->events_init(ops->context);
    if (rc != 0) {
        goto rollback;
    }
    events_initialized = true;

    rc = ops->bond_callout_init(ops->context);
    if (rc != 0) {
        goto rollback;
    }
    bond_callout_initialized = true;

    rc = ops->adv_callout_init(ops->context);
    if (rc != 0) {
        goto rollback;
    }
    adv_callout_initialized = true;

    rc = ops->gatt_init(ops->context);
    if (rc == 0) {
        return 0;
    }

rollback:
    if (adv_callout_initialized) {
        ops->adv_callout_deinit(ops->context);
    }
    if (bond_callout_initialized) {
        ops->bond_callout_deinit(ops->context);
    }
    if (events_initialized) {
        ops->events_deinit(ops->context);
    }
    if (host_initialized) {
        int deinit_rc = ops->host_deinit(ops->context);
        if (deinit_rc != 0) {
            *retry_safe = false;
            return deinit_rc;
        }
    }
    ops->clear(ops->context);
    return rc;
}

bool buddy_ble_lifecycle_retry_allowed(bool initialized, bool rollback_blocked)
{
    return !initialized && !rollback_blocked;
}
