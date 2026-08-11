#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "buddy_types.h"

#define BUDDY_EVENT_MALFORMED (-1)
#define BUDDY_EVENT_UNSUPPORTED_COMMAND (-2)
#define BUDDY_EVENT_UNKNOWN_COMMAND (-3)

int buddy_protocol_parse(const char *json, size_t length, buddy_event_t *event);
int buddy_protocol_permission_json(char *json, size_t size, const char *id,
                                   buddy_permission_decision_t decision);
int buddy_protocol_ack_json(char *json, size_t size, bool ok);
int buddy_protocol_status_json(char *json, size_t size, const buddy_heartbeat_t *heartbeat);
