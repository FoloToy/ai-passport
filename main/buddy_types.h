#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BUDDY_NAME_MAX 32
#define BUDDY_OWNER_MAX 32
#define BUDDY_MESSAGE_MAX 160
#define BUDDY_ENTRY_MAX 96
#define BUDDY_ENTRY_COUNT 4
#define BUDDY_PROMPT_ID_MAX 96
#define BUDDY_TOOL_MAX 48
#define BUDDY_HINT_MAX 320
#define BUDDY_JSON_LINE_MAX 4096

typedef enum {
    BUDDY_CONNECTION_OFFLINE,
    BUDDY_CONNECTION_CONNECTED,
    BUDDY_CONNECTION_PAIRING,
    BUDDY_CONNECTION_CONFIRMING,
} buddy_connection_t;

typedef enum {
    BUDDY_CHARACTER_SLEEP,
    BUDDY_CHARACTER_IDLE,
    BUDDY_CHARACTER_BUSY,
    BUDDY_CHARACTER_ATTENTION,
    BUDDY_CHARACTER_HEART,
    BUDDY_CHARACTER_CELEBRATE,
    BUDDY_CHARACTER_PAIRING,
    BUDDY_CHARACTER_CONFIRMATION,
} buddy_character_t;

typedef enum {
    BUDDY_PAGE_HOME,
    BUDDY_PAGE_SETTINGS,
} buddy_page_t;

typedef enum {
    BUDDY_KEY_NONE,
    BUDDY_KEY_UP,
    BUDDY_KEY_DOWN,
    BUDDY_KEY_OK,
    BUDDY_KEY_BACK,
} buddy_key_t;

typedef enum {
    BUDDY_EVENT_NONE,
    BUDDY_EVENT_HEARTBEAT,
    BUDDY_EVENT_PROMPT,
    BUDDY_EVENT_TIME,
    BUDDY_EVENT_NAME,
    BUDDY_EVENT_OWNER,
    BUDDY_EVENT_STATUS,
    BUDDY_EVENT_STATUS_REQUEST,
    BUDDY_EVENT_UNPAIR_CONFIRMATION,
    BUDDY_EVENT_KEY_CLICK,
    BUDDY_EVENT_KEY_LONG,
    BUDDY_EVENT_TICK,
} buddy_event_type_t;

typedef enum {
    BUDDY_ACTION_NONE,
    BUDDY_ACTION_UI_REFRESH,
    BUDDY_ACTION_PERMISSION,
    BUDDY_ACTION_SETTINGS,
    BUDDY_ACTION_STATUS,
    BUDDY_ACTION_UNPAIR_CONFIRMED,
} buddy_action_type_t;

typedef enum {
    BUDDY_PERMISSION_NONE,
    BUDDY_PERMISSION_ONCE,
    BUDDY_PERMISSION_ALWAYS,
    BUDDY_PERMISSION_DENY,
} buddy_permission_decision_t;

typedef struct {
    char name[BUDDY_NAME_MAX];
    char owner[BUDDY_OWNER_MAX];
    char time[BUDDY_MESSAGE_MAX];
    char message[BUDDY_MESSAGE_MAX];
    char entries[BUDDY_ENTRY_COUNT][BUDDY_ENTRY_MAX];
    unsigned running;
    uint64_t tokens;
    bool connected;
    bool name_truncated;
    bool owner_truncated;
    bool message_truncated;
    bool entries_truncated[BUDDY_ENTRY_COUNT];
} buddy_heartbeat_t;

typedef struct {
    char id[BUDDY_PROMPT_ID_MAX];
    char tool[BUDDY_TOOL_MAX];
    char hint[BUDDY_HINT_MAX];
    size_t id_length;
    unsigned running;
    bool id_truncated;
    bool tool_truncated;
    bool hint_truncated;
    bool connected;
} buddy_prompt_t;

typedef struct {
    char value[BUDDY_MESSAGE_MAX];
    bool value_truncated;
} buddy_command_t;

typedef struct {
    char name[BUDDY_NAME_MAX];
    char owner[BUDDY_OWNER_MAX];
    uint64_t approval_count;
    uint64_t denial_count;
    uint64_t highest_celebrated_level;
    bool ble_enabled;
} buddy_settings_snapshot_t;

typedef struct {
    buddy_event_type_t type;
    buddy_key_t key;
    buddy_heartbeat_t heartbeat;
    buddy_prompt_t prompt;
    buddy_command_t command;
    char observed_prompt_id[BUDDY_PROMPT_ID_MAX];
    size_t observed_prompt_id_length;
    bool has_observed_prompt_id;
    bool observed_prompt_id_truncated;
} buddy_event_t;

typedef struct {
    char id[BUDDY_PROMPT_ID_MAX];
    char tool[BUDDY_TOOL_MAX];
    char hint[BUDDY_HINT_MAX];
    buddy_permission_decision_t decision;
} buddy_permission_action_t;

typedef struct {
    buddy_action_type_t type;
    buddy_permission_action_t permission;
    buddy_settings_snapshot_t settings;
    char message[BUDDY_MESSAGE_MAX];
} buddy_action_t;

typedef struct {
    buddy_connection_t connection;
    buddy_character_t character;
    buddy_page_t page;
    char name[BUDDY_NAME_MAX];
    char owner[BUDDY_OWNER_MAX];
    char time[BUDDY_MESSAGE_MAX];
    char message[BUDDY_MESSAGE_MAX];
    char entries[BUDDY_ENTRY_COUNT][BUDDY_ENTRY_MAX];
    char prompt_id[BUDDY_PROMPT_ID_MAX];
    char prompt_tool[BUDDY_TOOL_MAX];
    char prompt_hint[BUDDY_HINT_MAX];
    unsigned running;
    uint64_t tokens;
    bool heartbeat_stale;
    bool confirmation_pending;
} buddy_ui_snapshot_t;
