#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "buddy_types.h"

#define BUDDY_EVENT_MALFORMED (-1)
#define BUDDY_EVENT_UNSUPPORTED_COMMAND (-2)
#define BUDDY_EVENT_UNKNOWN_COMMAND (-3)
#define BUDDY_JSON_MAX_DEPTH 16U
/* Worst-case JSON escaping for every protocol field emitted by Task 6. */
#define BUDDY_PROTOCOL_TX_MAX 768U

typedef struct {
    char name[BUDDY_NAME_MAX];
    char owner[BUDDY_OWNER_MAX];
    uint64_t uptime_ms;
    uint64_t free_heap;
    uint64_t approval_count;
    uint64_t denial_count;
    bool encrypted;
    bool battery_available;
    uint8_t battery_percent;
    uint16_t battery_mv;
} buddy_status_report_t;

int buddy_protocol_parse(const char *json, size_t length, buddy_event_t *event);
int buddy_protocol_permission_json(char *json, size_t size, const char *id,
                                   buddy_permission_decision_t decision);
int buddy_protocol_ack_json(char *json, size_t size, bool ok);
int buddy_protocol_status_json(char *json, size_t size, const buddy_heartbeat_t *heartbeat);
int buddy_protocol_command_ack_json(char *json, size_t size, const char *command,
                                    bool ok, const char *error);
int buddy_protocol_device_status_json(char *json, size_t size,
                                      const buddy_status_report_t *status);
