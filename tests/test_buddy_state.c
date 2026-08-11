#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "buddy_state.h"

static buddy_event_t test_prompt_event(const char *id, const char *tool,
                                       const char *hint, unsigned running,
                                       unsigned connected)
{
    buddy_event_t event = {0};

    event.type = BUDDY_EVENT_PROMPT;
    snprintf(event.prompt.id, sizeof(event.prompt.id), "%s", id);
    snprintf(event.prompt.tool, sizeof(event.prompt.tool), "%s", tool);
    snprintf(event.prompt.hint, sizeof(event.prompt.hint), "%s", hint);
    event.prompt.running = running;
    event.prompt.connected = connected != 0;
    return event;
}

static buddy_event_t test_heartbeat_event(uint64_t tokens, unsigned running)
{
    buddy_event_t event = {0};

    event.type = BUDDY_EVENT_HEARTBEAT;
    event.heartbeat.connected = true;
    event.heartbeat.running = running;
    event.heartbeat.tokens = tokens;
    snprintf(event.heartbeat.owner, sizeof(event.heartbeat.owner), "%s", "Claude");
    return event;
}

static void test_offline_initialization(void)
{
    buddy_state_t state;
    buddy_ui_snapshot_t snapshot;

    buddy_state_init(&state, NULL);
    buddy_state_snapshot(&state, &snapshot);

    assert(state.connection == BUDDY_CONNECTION_OFFLINE);
    assert(snapshot.character == BUDDY_CHARACTER_SLEEP);
    assert(snapshot.page == BUDDY_PAGE_HOME);
}

static void test_heartbeat_mapping(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t heartbeat = test_heartbeat_event(1234, 2);
    buddy_ui_snapshot_t snapshot;

    buddy_state_init(&state, NULL);
    buddy_state_reduce(&state, &heartbeat, 1000, &action);
    buddy_state_snapshot(&state, &snapshot);

    assert(action.type == BUDDY_ACTION_UI_REFRESH);
    assert(state.connection == BUDDY_CONNECTION_CONNECTED);
    assert(state.tokens == 1234);
    assert(state.running == 2);
    assert(strcmp(snapshot.owner, "Claude") == 0);
    assert(snapshot.character == BUDDY_CHARACTER_BUSY);
}

static void test_character_priority(void)
{
    struct priority_case {
        buddy_connection_t connection;
        bool has_prompt;
        bool temporary;
        unsigned running;
        buddy_character_t want;
    } cases[] = {
        {BUDDY_CONNECTION_PAIRING, true, true, 1, BUDDY_CHARACTER_PAIRING},
        {BUDDY_CONNECTION_CONFIRMING, true, true, 1, BUDDY_CHARACTER_CONFIRMATION},
        {BUDDY_CONNECTION_CONNECTED, true, true, 1, BUDDY_CHARACTER_ATTENTION},
        {BUDDY_CONNECTION_CONNECTED, false, true, 1, BUDDY_CHARACTER_HEART},
        {BUDDY_CONNECTION_CONNECTED, false, false, 1, BUDDY_CHARACTER_BUSY},
        {BUDDY_CONNECTION_CONNECTED, false, false, 0, BUDDY_CHARACTER_IDLE},
        {BUDDY_CONNECTION_OFFLINE, false, false, 0, BUDDY_CHARACTER_SLEEP},
    };
    size_t index;

    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        buddy_state_t state;
        buddy_ui_snapshot_t snapshot;

        buddy_state_init(&state, NULL);
        state.connection = cases[index].connection;
        state.connected = cases[index].connection == BUDDY_CONNECTION_CONNECTED;
        state.heartbeat_stale = false;
        state.running = cases[index].running;
        if (cases[index].has_prompt) {
            snprintf(state.prompt.id, sizeof(state.prompt.id), "%s", "req-1");
        }
        if (cases[index].temporary) {
            state.temporary_character = BUDDY_CHARACTER_HEART;
            state.temporary_until_ms = 1001;
        }

        buddy_event_t tick = {.type = BUDDY_EVENT_TICK};
        buddy_state_reduce(&state, &tick, 1000, NULL);
        buddy_state_snapshot(&state, &snapshot);
        assert(snapshot.character == cases[index].want);
    }
}

static void test_timeout_clears_prompt(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t heartbeat = test_heartbeat_event(0, 0);
    buddy_event_t tick = {.type = BUDDY_EVENT_TICK};

    buddy_state_init(&state, NULL);
    buddy_state_reduce(&state, &heartbeat, 1000, &action);
    buddy_event_t prompt = test_prompt_event("req-1", "Bash", "git push", 0, 1);
    buddy_state_reduce(&state, &prompt, 1001, &action);
    memset(&action, 0, sizeof(action));
    buddy_state_reduce(&state, &tick, 31000, &action);

    assert(action.type == BUDDY_ACTION_UI_REFRESH);
    assert(state.heartbeat_stale);
    assert(state.prompt.id[0] == '\0');
    assert(state.character == BUDDY_CHARACTER_SLEEP);
}

static void test_token_boundaries_celebrate_once(void)
{
    buddy_state_t state;
    buddy_settings_snapshot_t settings = {.highest_celebrated_level = 0};
    buddy_action_t action = {0};
    buddy_event_t heartbeat = test_heartbeat_event(49999, 0);

    buddy_state_init(&state, &settings);
    buddy_state_reduce(&state, &heartbeat, 1000, &action);
    assert(action.type == BUDDY_ACTION_UI_REFRESH);

    heartbeat = test_heartbeat_event(50000, 0);
    buddy_state_reduce(&state, &heartbeat, 1001, &action);
    assert(action.type == BUDDY_ACTION_SETTINGS);
    assert(action.settings.highest_celebrated_level == 1);
    assert(state.character == BUDDY_CHARACTER_CELEBRATE);

    memset(&action, 0, sizeof(action));
    buddy_state_reduce(&state, &heartbeat, 1002, &action);
    assert(action.type == BUDDY_ACTION_UI_REFRESH);
    assert(state.highest_celebrated_level == 1);

    heartbeat = test_heartbeat_event(100000, 0);
    buddy_state_reduce(&state, &heartbeat, 1003, &action);
    assert(action.type == BUDDY_ACTION_SETTINGS);
    assert(action.settings.highest_celebrated_level == 2);
}

static void test_persisted_celebration_level_is_not_replayed(void)
{
    buddy_state_t state;
    buddy_settings_snapshot_t settings = {.highest_celebrated_level = 2};
    buddy_action_t action = {0};
    buddy_event_t heartbeat = test_heartbeat_event(100000, 0);

    buddy_state_init(&state, &settings);
    buddy_state_reduce(&state, &heartbeat, 1000, &action);

    assert(action.type == BUDDY_ACTION_UI_REFRESH);
    assert(state.highest_celebrated_level == 2);
}

static void test_approval_locks_until_a_new_prompt(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t prompt;
    buddy_event_t approve = {.type = BUDDY_EVENT_KEY_CLICK, .key = BUDDY_KEY_OK};

    buddy_state_init(&state, NULL);

    prompt = test_prompt_event("req-1", "Bash", "git push", 1, 1);
    buddy_state_reduce(&state, &prompt, 1000, &action);
    assert(state.character == BUDDY_CHARACTER_ATTENTION);

    buddy_state_reduce(&state, &approve, 2000, &action);
    assert(action.type == BUDDY_ACTION_PERMISSION);
    assert(strcmp(action.permission.id, "req-1") == 0);
    assert(action.permission.decision == BUDDY_PERMISSION_ONCE);
    assert(state.character == BUDDY_CHARACTER_HEART);

    memset(&action, 0, sizeof(action));
    buddy_state_reduce(&state, &approve, 2100, &action);
    assert(action.type == BUDDY_ACTION_NONE);

    prompt = test_prompt_event("req-2", "Bash", "git status", 1, 1);
    buddy_state_reduce(&state, &prompt, 2200, &action);
    buddy_state_reduce(&state, &approve, 2201, &action);
    assert(action.type == BUDDY_ACTION_PERMISSION);
    assert(strcmp(action.permission.id, "req-2") == 0);
}

static void test_stale_or_mismatched_prompt_is_ignored(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t prompt = test_prompt_event("req-1", "Bash", "git push", 0, 0);
    buddy_event_t approve = {.type = BUDDY_EVENT_KEY_CLICK, .key = BUDDY_KEY_OK};

    buddy_state_init(&state, NULL);
    buddy_state_reduce(&state, &prompt, 1000, &action);
    buddy_state_reduce(&state, &approve, 1001, &action);
    assert(action.type == BUDDY_ACTION_NONE);
}

static void test_long_ok_opens_settings(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t long_ok = {.type = BUDDY_EVENT_KEY_LONG, .key = BUDDY_KEY_OK};

    buddy_state_init(&state, NULL);
    buddy_state_reduce(&state, &long_ok, 1000, &action);

    assert(state.page == BUDDY_PAGE_SETTINGS);
    assert(action.type == BUDDY_ACTION_UI_REFRESH);
}

int main(void)
{
    test_offline_initialization();
    test_heartbeat_mapping();
    test_character_priority();
    test_timeout_clears_prompt();
    test_token_boundaries_celebrate_once();
    test_persisted_celebration_level_is_not_replayed();
    test_approval_locks_until_a_new_prompt();
    test_stale_or_mismatched_prompt_is_ignored();
    test_long_ok_opens_settings();
    return 0;
}
