#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#include "buddy_protocol.h"
#include "buddy_state.h"

typedef struct {
    void *context;
    bool (*generation_secure)(void *context, uint32_t generation);
    esp_err_t (*send)(void *context, const char *data, size_t length,
                      uint32_t generation);
    esp_err_t (*commit_name)(void *context, const char *name);
    esp_err_t (*commit_owner)(void *context, const char *owner);
    esp_err_t (*status_report)(void *context, const buddy_state_t *state,
                               buddy_status_report_t *report);
    void (*record_permission)(void *context, buddy_permission_decision_t decision);
    esp_err_t (*unpair)(void *context);
    esp_err_t (*factory_reset)(void *context);
    esp_err_t (*set_ble_enabled)(void *context, bool enabled);
    esp_err_t (*persist_level)(void *context, uint64_t level);
} buddy_orchestrator_ops_t;

bool buddy_orchestrator_process_rx(buddy_state_t *state,
                                   const buddy_orchestrator_ops_t *ops,
                                   const char *json, size_t length,
                                   uint32_t connection_generation,
                                   uint64_t now_ms, buddy_action_t *action);
bool buddy_orchestrator_execute_action(buddy_state_t *state,
                                       const buddy_orchestrator_ops_t *ops,
                                       const buddy_action_t *action,
                                       buddy_event_t *result_event);
