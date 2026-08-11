#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "buddy_state.h"

static buddy_event_t test_prompt_event(const char *id, const char *tool,
                                       const char *hint, unsigned running,
                                       unsigned connected)
{
    buddy_event_t event = {0};
    size_t id_length = strlen(id);

    event.type = BUDDY_EVENT_PROMPT;
    event.prompt.id_length = id_length;
    event.prompt.id_truncated = id_length >= sizeof(event.prompt.id);
    snprintf(event.prompt.id, sizeof(event.prompt.id), "%s", id);
    snprintf(event.prompt.tool, sizeof(event.prompt.tool), "%s", tool);
    snprintf(event.prompt.hint, sizeof(event.prompt.hint), "%s", hint);
    event.prompt.running = running;
    event.prompt.connected = connected != 0;
    return event;
}

static void test_set_observed_prompt_id(buddy_event_t *event, const char *id)
{
    size_t id_length = strlen(id);

    event->has_observed_prompt_id = true;
    event->observed_prompt_id_length = id_length;
    event->observed_prompt_id_truncated = id_length >= sizeof(event->observed_prompt_id);
    snprintf(event->observed_prompt_id, sizeof(event->observed_prompt_id), "%s", id);
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

static void test_absent_prompt_is_ignored(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t approve = {.type = BUDDY_EVENT_KEY_CLICK, .key = BUDDY_KEY_OK};

    buddy_state_init(&state, NULL);
    buddy_state_reduce(&state, &approve, 1001, &action);
    assert(action.type == BUDDY_ACTION_NONE);
}

static void test_timeout_stale_prompt_is_ignored(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t heartbeat = test_heartbeat_event(0, 0);
    buddy_event_t prompt = test_prompt_event("req-1", "Bash", "git push", 0, 1);
    buddy_event_t tick = {.type = BUDDY_EVENT_TICK};
    buddy_event_t approve = {.type = BUDDY_EVENT_KEY_CLICK, .key = BUDDY_KEY_OK};

    buddy_state_init(&state, NULL);
    buddy_state_reduce(&state, &heartbeat, 1000, &action);
    buddy_state_reduce(&state, &prompt, 1001, &action);
    buddy_state_reduce(&state, &tick, 31000, &action);
    buddy_state_reduce(&state, &approve, 31001, &action);
    assert(action.type == BUDDY_ACTION_NONE);
}

static void test_disconnect_then_reconnect_does_not_restore_prompt(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t heartbeat = test_heartbeat_event(0, 0);
    buddy_event_t prompt = test_prompt_event("req-1", "Bash", "git push", 0, 1);
    buddy_event_t approve = {.type = BUDDY_EVENT_KEY_CLICK, .key = BUDDY_KEY_OK};

    buddy_state_init(&state, NULL);
    buddy_state_reduce(&state, &heartbeat, 1000, &action);
    buddy_state_reduce(&state, &prompt, 1001, &action);

    heartbeat.heartbeat.connected = false;
    buddy_state_reduce(&state, &heartbeat, 1002, &action);
    heartbeat.heartbeat.connected = true;
    buddy_state_reduce(&state, &heartbeat, 1003, &action);
    buddy_state_reduce(&state, &approve, 1004, &action);

    assert(state.prompt.id[0] == '\0');
    assert(action.type == BUDDY_ACTION_NONE);
}

static void test_mismatched_observed_prompt_is_ignored(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t prompt = test_prompt_event("req-1", "Bash", "git push", 0, 1);
    buddy_event_t approve = {.type = BUDDY_EVENT_KEY_CLICK, .key = BUDDY_KEY_OK};

    buddy_state_init(&state, NULL);
    buddy_state_reduce(&state, &prompt, 1000, &action);
    test_set_observed_prompt_id(&approve, "req-2");
    buddy_state_reduce(&state, &approve, 1001, &action);

    assert(action.type == BUDDY_ACTION_NONE);
    assert(strcmp(state.prompt.id, "req-1") == 0);
}

static void test_nonterminated_prompt_id_is_ignored(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t prompt = test_prompt_event("req-1", "Bash", "git push", 0, 1);
    buddy_event_t approve = {.type = BUDDY_EVENT_KEY_CLICK, .key = BUDDY_KEY_OK};

    memset(prompt.prompt.id, 'a', sizeof(prompt.prompt.id));
    prompt.prompt.id_length = sizeof(prompt.prompt.id);
    buddy_state_init(&state, NULL);
    buddy_state_reduce(&state, &prompt, 1000, &action);
    buddy_state_reduce(&state, &approve, 1001, &action);

    assert(action.type == BUDDY_ACTION_NONE);
}

static void test_truncated_prompt_id_is_ignored(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t prompt = test_prompt_event("req-1", "Bash", "git push", 0, 1);
    buddy_event_t approve = {.type = BUDDY_EVENT_KEY_CLICK, .key = BUDDY_KEY_OK};

    prompt.prompt.id_truncated = true;
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

static void test_protocol_command_events_refresh_the_display(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t event = {.type = BUDDY_EVENT_NAME};

    buddy_state_init(&state, NULL);
    snprintf(event.command.value, sizeof(event.command.value), "%s", "Buddy");
    buddy_state_reduce(&state, &event, 1000, &action);
    assert(strcmp(state.name, "Buddy") == 0);
    assert(action.type == BUDDY_ACTION_UI_REFRESH);

    event.type = BUDDY_EVENT_OWNER;
    snprintf(event.command.value, sizeof(event.command.value), "%s", "Claude");
    buddy_state_reduce(&state, &event, 1001, &action);
    assert(strcmp(state.owner, "Claude") == 0);

    event.type = BUDDY_EVENT_STATUS;
    snprintf(event.command.value, sizeof(event.command.value), "%s", "Ready");
    buddy_state_reduce(&state, &event, 1002, &action);
    assert(strcmp(state.message, "Ready") == 0);

    event.type = BUDDY_EVENT_TIME;
    snprintf(event.command.value, sizeof(event.command.value), "%s", "12:34");
    buddy_state_reduce(&state, &event, 1003, &action);
    assert(strcmp(state.time, "12:34") == 0);

    event.type = BUDDY_EVENT_UNPAIR_CONFIRMATION;
    buddy_state_reduce(&state, &event, 1004, &action);
    assert(state.connection == BUDDY_CONNECTION_CONFIRMING);
    assert(state.character == BUDDY_CHARACTER_CONFIRMATION);
}

static void test_status_request_does_not_overwrite_message(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t event = {.type = BUDDY_EVENT_STATUS_REQUEST};

    buddy_state_init(&state, NULL);
    snprintf(state.message, sizeof(state.message), "%s", "Keep this");
    buddy_state_reduce(&state, &event, 1000, &action);

    assert(action.type == BUDDY_ACTION_STATUS);
    assert(strcmp(state.message, "Keep this") == 0);
}

static void test_unpair_confirmation_survives_a_heartbeat(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t unpair = {.type = BUDDY_EVENT_UNPAIR_CONFIRMATION};
    buddy_event_t heartbeat = test_heartbeat_event(0, 0);

    buddy_state_init(&state, NULL);
    buddy_state_reduce(&state, &unpair, 1000, &action);
    buddy_state_reduce(&state, &heartbeat, 1001, &action);

    assert(state.confirmation_pending);
    assert(state.connection == BUDDY_CONNECTION_CONNECTED);
    assert(state.character == BUDDY_CHARACTER_CONFIRMATION);
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
    test_absent_prompt_is_ignored();
    test_timeout_stale_prompt_is_ignored();
    test_disconnect_then_reconnect_does_not_restore_prompt();
    test_mismatched_observed_prompt_is_ignored();
    test_nonterminated_prompt_id_is_ignored();
    test_truncated_prompt_id_is_ignored();
    test_long_ok_opens_settings();
    test_protocol_command_events_refresh_the_display();
    test_status_request_does_not_overwrite_message();
    test_unpair_confirmation_survives_a_heartbeat();
    return 0;
}
