#pragma once

#include "buddy_types.h"

typedef struct {
    buddy_connection_t connection;
    buddy_character_t character;
    buddy_page_t page;
    buddy_heartbeat_t heartbeat;
    buddy_prompt_t prompt;
    buddy_settings_snapshot_t settings;
    char last_approved_prompt_id[BUDDY_PROMPT_ID_MAX];
    char name[BUDDY_NAME_MAX];
    char owner[BUDDY_OWNER_MAX];
    char time[BUDDY_MESSAGE_MAX];
    char message[BUDDY_MESSAGE_MAX];
    char entries[BUDDY_ENTRY_COUNT][BUDDY_ENTRY_MAX];
    unsigned running;
    uint64_t tokens;
    uint64_t highest_celebrated_level;
    uint64_t last_heartbeat_ms;
    uint64_t temporary_until_ms;
    buddy_character_t temporary_character;
    bool connected;
    bool heartbeat_stale;
    bool confirmation_pending;
    bool approval_locked;
} buddy_state_t;

void buddy_state_init(buddy_state_t *state, const buddy_settings_snapshot_t *settings);
void buddy_state_reduce(buddy_state_t *state, const buddy_event_t *event,
                        uint64_t now_ms, buddy_action_t *action);
void buddy_state_snapshot(const buddy_state_t *state, buddy_ui_snapshot_t *snapshot);
