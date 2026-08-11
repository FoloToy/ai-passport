#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#include "buddy_protocol.h"
#include "buddy_types.h"

typedef enum {
    BUDDY_APP_RX_NORMAL_HEARTBEAT,
    BUDDY_APP_RX_PRIORITY,
} buddy_app_rx_class_t;

typedef enum {
    BUDDY_APP_RX_ENQUEUE,
    BUDDY_APP_RX_REPLACE_NORMAL,
    BUDDY_APP_RX_REPLACE_OLDEST_PRIORITY,
    BUDDY_APP_RX_DROP,
} buddy_app_rx_overflow_action_t;

typedef struct {
    bool encrypted;
    bool battery_available;
    uint8_t battery_percent;
    uint16_t battery_mv;
    uint64_t uptime_ms;
    uint64_t free_heap;
    uint64_t queue_overflow_count;
} buddy_app_status_runtime_t;

typedef esp_err_t (*buddy_app_ble_transport_fn_t)(void *context);

typedef struct {
    void *context;
    buddy_app_ble_transport_fn_t start;
    buddy_app_ble_transport_fn_t stop;
} buddy_app_ble_transport_ops_t;

typedef struct {
    esp_err_t request_status;
    bool effective_enabled;
    bool recovery_attempted;
} buddy_app_ble_transport_result_t;

buddy_app_rx_class_t buddy_app_classify_rx(const char *data, size_t length);
buddy_app_rx_overflow_action_t buddy_app_rx_overflow_policy(
    buddy_app_rx_class_t incoming, bool slot_available, bool normal_pending,
    bool priority_pending, bool priority_full);
bool buddy_app_build_status(buddy_status_report_t *report,
                            const buddy_settings_snapshot_t *settings,
                            const buddy_app_status_runtime_t *runtime);
buddy_app_ble_transport_result_t buddy_app_set_ble_transport(
    const buddy_app_ble_transport_ops_t *ops, bool enabled);
