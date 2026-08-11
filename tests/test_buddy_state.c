#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "buddy_protocol.h"
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

static buddy_event_t test_permission_result_event(const char *id,
                                                   buddy_permission_decision_t decision,
                                                   bool success)
{
    buddy_event_t event = {0};

    event.type = BUDDY_EVENT_PERMISSION_SEND_RESULT;
    event.permission_result.id_length = strlen(id);
    snprintf(event.permission_result.id, sizeof(event.permission_result.id), "%s", id);
    event.permission_result.decision = decision;
    event.permission_result.success = success;
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
    buddy_state_reduce(&state, &tick, 31001, &action);

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
    buddy_ui_snapshot_t snapshot;
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
    assert(state.character != BUDDY_CHARACTER_HEART);
    buddy_state_snapshot(&state, &snapshot);
    assert(snapshot.approval_locked);
    assert(strcmp(snapshot.prompt_id, "req-1") == 0);

    memset(&action, 0, sizeof(action));
    buddy_state_reduce(&state, &approve, 2100, &action);
    assert(action.type == BUDDY_ACTION_NONE);

    prompt = test_prompt_event("req-2", "Bash", "git status", 1, 1);
    buddy_state_reduce(&state, &prompt, 2200, &action);
    buddy_state_snapshot(&state, &snapshot);
    assert(!snapshot.approval_locked);
    buddy_state_reduce(&state, &approve, 2201, &action);
    assert(action.type == BUDDY_ACTION_PERMISSION);
    assert(strcmp(action.permission.id, "req-2") == 0);
}

static void test_denial_locks_and_emits_exactly_once(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t prompt = test_prompt_event("req-deny", "Bash", "rm guarded", 1, 1);
    buddy_event_t deny = {.type = BUDDY_EVENT_KEY_CLICK, .key = BUDDY_KEY_DOWN};

    buddy_state_init(&state, NULL);
    buddy_state_reduce(&state, &prompt, 1000, &action);
    buddy_state_reduce(&state, &deny, 1001, &action);

    assert(action.type == BUDDY_ACTION_PERMISSION);
    assert(strcmp(action.permission.id, "req-deny") == 0);
    assert(action.permission.decision == BUDDY_PERMISSION_DENY);
    assert(state.approval_locked);

    buddy_state_reduce(&state, &deny, 1002, &action);
    assert(action.type == BUDDY_ACTION_NONE);
}

static void test_permission_action_is_bound_to_the_prompt_connection(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t prompt = test_prompt_event("req-generation", "Bash", "git push", 1, 1);
    buddy_event_t approve = {.type = BUDDY_EVENT_KEY_CLICK, .key = BUDDY_KEY_OK};

    prompt.ble.connection_generation = 7;
    buddy_state_init(&state, NULL);
    buddy_state_reduce(&state, &prompt, 1000, &action);
    buddy_state_reduce(&state, &approve, 1001, &action);

    assert(action.type == BUDDY_ACTION_PERMISSION);
    assert(action.permission.connection_generation == 7);
}

static void test_permission_success_moves_from_attempted_to_successful_state(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_ui_snapshot_t snapshot;
    buddy_event_t prompt = test_prompt_event("req-success", "Bash", "git push", 1, 1);
    buddy_event_t approve = {.type = BUDDY_EVENT_KEY_CLICK, .key = BUDDY_KEY_OK};
    buddy_event_t result = test_permission_result_event(
        "req-success", BUDDY_PERMISSION_ONCE, true);

    buddy_state_init(&state, NULL);
    buddy_state_reduce(&state, &prompt, 1000, &action);
    buddy_state_reduce(&state, &approve, 1001, &action);
    assert(action.type == BUDDY_ACTION_PERMISSION);
    assert(strcmp(state.last_attempted_prompt_id, "req-success") == 0);
    assert(state.last_successful_decision_id[0] == '\0');
    assert(state.permission_delivery == BUDDY_PERMISSION_DELIVERY_SENDING);
    assert(state.character != BUDDY_CHARACTER_HEART);

    buddy_state_reduce(&state, &result, 1002, &action);
    buddy_state_snapshot(&state, &snapshot);
    assert(action.type == BUDDY_ACTION_UI_REFRESH);
    assert(strcmp(state.last_successful_decision_id, "req-success") == 0);
    assert(snapshot.permission_delivery == BUDDY_PERMISSION_DELIVERY_SENT);
    assert(state.character == BUDDY_CHARACTER_HEART);
}

static void test_permission_failure_is_visible_and_same_id_replay_stays_locked(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_ui_snapshot_t snapshot;
    buddy_event_t prompt = test_prompt_event("req-failed", "Bash", "git push", 1, 1);
    buddy_event_t approve = {.type = BUDDY_EVENT_KEY_CLICK, .key = BUDDY_KEY_OK};
    buddy_event_t failure = test_permission_result_event(
        "req-failed", BUDDY_PERMISSION_ONCE, false);

    buddy_state_init(&state, NULL);
    buddy_state_reduce(&state, &prompt, 1000, &action);
    buddy_state_reduce(&state, &approve, 1001, &action);
    buddy_state_reduce(&state, &failure, 1002, &action);
    buddy_state_snapshot(&state, &snapshot);
    assert(snapshot.permission_delivery == BUDDY_PERMISSION_DELIVERY_FAILED);
    assert(strcmp(state.last_attempted_prompt_id, "req-failed") == 0);
    assert(state.last_successful_decision_id[0] == '\0');
    assert(state.approval_locked);
    assert(state.character != BUDDY_CHARACTER_HEART);

    buddy_state_reduce(&state, &prompt, 1003, &action);
    buddy_state_reduce(&state, &approve, 1004, &action);
    assert(action.type == BUDDY_ACTION_NONE);
    assert(state.permission_delivery == BUDDY_PERMISSION_DELIVERY_FAILED);
}

static void test_heartbeat_prompt_snapshot_clears_or_preserves_approval_lock(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_ui_snapshot_t snapshot;
    buddy_event_t heartbeat = test_heartbeat_event(0, 0);
    buddy_event_t prompt = test_prompt_event("req-1", "Bash", "git push", 0, 1);
    buddy_event_t approve = {.type = BUDDY_EVENT_KEY_CLICK, .key = BUDDY_KEY_OK};

    buddy_state_init(&state, NULL);
    buddy_state_reduce(&state, &prompt, 1000, &action);
    buddy_state_reduce(&state, &approve, 1001, &action);
    assert(state.approval_locked);

    heartbeat.heartbeat.connected = true;
    buddy_state_reduce(&state, &heartbeat, 1002, &action);
    buddy_state_snapshot(&state, &snapshot);
    assert(!snapshot.approval_locked);
    assert(snapshot.prompt_id[0] == '\0');

    heartbeat.heartbeat.prompt = prompt.prompt;
    buddy_state_reduce(&state, &heartbeat, 1003, &action);
    buddy_state_snapshot(&state, &snapshot);
    assert(snapshot.prompt_id[0] == '\0');
    buddy_state_reduce(&state, &approve, 1004, &action);
    assert(action.type == BUDDY_ACTION_NONE);

    heartbeat.heartbeat.prompt = test_prompt_event("req-2", "Read", "README", 0, 1).prompt;
    buddy_state_reduce(&state, &heartbeat, 1005, &action);
    buddy_state_snapshot(&state, &snapshot);
    assert(!snapshot.approval_locked);
    assert(strcmp(snapshot.prompt_id, "req-2") == 0);
    buddy_state_reduce(&state, &approve, 1006, &action);
    assert(action.type == BUDDY_ACTION_PERMISSION);
    assert(strcmp(action.permission.id, "req-2") == 0);
}

static void test_ui_snapshot_runtime_indicators_default_off(void)
{
    buddy_state_t state;
    buddy_ui_snapshot_t snapshot;

    buddy_state_init(&state, NULL);
    buddy_state_snapshot(&state, &snapshot);

    assert(!snapshot.ble_connected);
    assert(!snapshot.ble_encrypted);
    assert(!snapshot.battery_available);
    assert(snapshot.battery_percent == 0);
    assert(snapshot.battery_mv == 0);

    state.ble_connected = true;
    state.ble_encrypted = true;
    state.battery_available = true;
    state.battery_percent = 73;
    state.battery_mv = 3875;
    buddy_state_snapshot(&state, &snapshot);
    assert(snapshot.ble_connected);
    assert(snapshot.ble_encrypted);
    assert(snapshot.battery_available);
    assert(snapshot.battery_percent == 73);
    assert(snapshot.battery_mv == 3875);
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
    buddy_state_reduce(&state, &tick, 31001, &action);
    buddy_state_reduce(&state, &approve, 31002, &action);
    assert(action.type == BUDDY_ACTION_NONE);
}

static void test_standalone_prompt_refreshes_liveness_clock(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t prompt = test_prompt_event("req-late", "Bash", "git status", 0, 1);
    buddy_event_t approve = {.type = BUDDY_EVENT_KEY_CLICK, .key = BUDDY_KEY_OK};

    buddy_state_init(&state, NULL);
    buddy_state_reduce(&state, &prompt, 60000, &action);
    buddy_state_reduce(&state, &approve, 60001, &action);

    assert(action.type == BUDDY_ACTION_PERMISSION);
    assert(strcmp(action.permission.id, "req-late") == 0);
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
    event.ble.connection_generation = 11;
    snprintf(state.message, sizeof(state.message), "%s", "Keep this");
    buddy_state_reduce(&state, &event, 1000, &action);

    assert(action.type == BUDDY_ACTION_STATUS);
    assert(action.connection_generation == 11);
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

static void test_unpair_confirmation_ok_emits_explicit_action(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t unpair = {.type = BUDDY_EVENT_UNPAIR_CONFIRMATION};
    buddy_event_t heartbeat = test_heartbeat_event(0, 0);
    buddy_event_t ok = {.type = BUDDY_EVENT_KEY_CLICK, .key = BUDDY_KEY_OK};

    buddy_state_init(&state, NULL);
    buddy_state_reduce(&state, &unpair, 1000, &action);
    buddy_state_reduce(&state, &heartbeat, 1001, &action);
    buddy_state_reduce(&state, &ok, 1002, &action);

    assert(!state.confirmation_pending);
    assert(action.type == BUDDY_ACTION_UNPAIR_CONFIRMED);
}

static void test_unpair_confirmation_down_cancels_without_action(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t unpair = {.type = BUDDY_EVENT_UNPAIR_CONFIRMATION};
    buddy_event_t down = {.type = BUDDY_EVENT_KEY_CLICK, .key = BUDDY_KEY_DOWN};

    buddy_state_init(&state, NULL);
    buddy_state_reduce(&state, &unpair, 1000, &action);
    buddy_state_reduce(&state, &down, 1001, &action);

    assert(!state.confirmation_pending);
    assert(action.type == BUDDY_ACTION_UI_REFRESH);
}

static void test_parsed_heartbeat_approval_serializes_permission(void)
{
    const char *json =
        "{\"cmd\":\"heartbeat\",\"running\":1,\"prompt\":{"
        "\"id\":\"req-e2e\",\"tool\":\"Bash\",\"hint\":\"git status\"}}";
    buddy_state_t state;
    buddy_event_t heartbeat = {0};
    buddy_event_t approve = {.type = BUDDY_EVENT_KEY_CLICK, .key = BUDDY_KEY_OK};
    buddy_action_t action = {0};
    char output[160];

    assert(buddy_protocol_parse(json, strlen(json), &heartbeat) == BUDDY_EVENT_HEARTBEAT);
    buddy_state_init(&state, NULL);
    buddy_state_reduce(&state, &heartbeat, 1000, &action);
    buddy_state_reduce(&state, &approve, 1001, &action);

    assert(action.type == BUDDY_ACTION_PERMISSION);
    assert(buddy_protocol_permission_json(output, sizeof(output), action.permission.id,
                                          action.permission.decision) > 0);
    assert(strcmp(output,
                  "{\"cmd\":\"permission\",\"id\":\"req-e2e\",\"decision\":\"once\"}\n") ==
           0);
}

static void test_normal_navigation_and_approval_scroll_are_distinct(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t down = {.type = BUDDY_EVENT_KEY_CLICK, .key = BUDDY_KEY_DOWN};
    buddy_event_t up = {.type = BUDDY_EVENT_KEY_CLICK, .key = BUDDY_KEY_UP};
    buddy_event_t prompt = test_prompt_event("req-scroll", "Read", "long hint", 0, 1);

    buddy_state_init(&state, NULL);
    buddy_state_reduce(&state, &down, 1000, &action);
    assert(state.page == BUDDY_PAGE_TRANSCRIPT);
    assert(action.type == BUDDY_ACTION_UI_REFRESH);
    buddy_state_reduce(&state, &up, 1001, &action);
    assert(state.page == BUDDY_PAGE_HOME);

    buddy_state_reduce(&state, &prompt, 1002, &action);
    buddy_state_reduce(&state, &up, 1003, &action);
    assert(action.type == BUDDY_ACTION_UI_SCROLL);
    assert(action.scroll_delta < 0);
}

static void test_settings_actions_have_separate_confirmations(void)
{
    buddy_settings_snapshot_t settings = {.ble_enabled = true};
    buddy_state_t state;
    buddy_ui_snapshot_t snapshot;
    buddy_action_t action = {0};
    buddy_event_t long_ok = {.type = BUDDY_EVENT_KEY_LONG, .key = BUDDY_KEY_OK};
    buddy_event_t click_ok = {.type = BUDDY_EVENT_KEY_CLICK, .key = BUDDY_KEY_OK};
    buddy_event_t click_down = {.type = BUDDY_EVENT_KEY_CLICK, .key = BUDDY_KEY_DOWN};

    buddy_state_init(&state, &settings);
    buddy_state_reduce(&state, &long_ok, 1000, &action);
    buddy_state_reduce(&state, &click_ok, 1001, &action);
    assert(action.type == BUDDY_ACTION_BLE_TOGGLE);
    assert(!action.ble_enabled);
    buddy_state_snapshot(&state, &snapshot);
    assert(!snapshot.ble_enabled);

    buddy_state_reduce(&state, &click_down, 1002, &action);
    buddy_state_reduce(&state, &click_ok, 1003, &action);
    assert(state.confirmation == BUDDY_CONFIRM_UNPAIR);
    assert(!state.confirmation_acknowledge);
    buddy_state_reduce(&state, &click_down, 1004, &action);

    buddy_state_reduce(&state, &long_ok, 1005, &action);
    buddy_state_reduce(&state, &click_down, 1006, &action);
    buddy_state_reduce(&state, &click_down, 1007, &action);
    buddy_state_reduce(&state, &click_ok, 1008, &action);
    assert(state.confirmation == BUDDY_CONFIRM_FACTORY_RESET);
    buddy_state_reduce(&state, &click_ok, 1009, &action);
    assert(action.type == BUDDY_ACTION_FACTORY_RESET_CONFIRMED);
}

static void test_remote_unpair_confirmation_remembers_ack(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t unpair = {.type = BUDDY_EVENT_UNPAIR_CONFIRMATION};
    buddy_event_t ok = {.type = BUDDY_EVENT_KEY_CLICK, .key = BUDDY_KEY_OK};

    buddy_state_init(&state, NULL);
    unpair.ble.connection_generation = 13;
    buddy_state_reduce(&state, &unpair, 1000, &action);
    assert(state.confirmation == BUDDY_CONFIRM_UNPAIR);
    assert(state.confirmation_acknowledge);
    buddy_state_reduce(&state, &ok, 1001, &action);
    assert(action.type == BUDDY_ACTION_UNPAIR_CONFIRMED);
    assert(action.confirmation_acknowledge);
    assert(action.connection_generation == 13);
}

static void test_remote_unpair_cannot_replace_a_local_confirmation(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t remote_unpair = {.type = BUDDY_EVENT_UNPAIR_CONFIRMATION};

    buddy_state_init(&state, NULL);
    state.confirmation = BUDDY_CONFIRM_FACTORY_RESET;
    state.confirmation_pending = true;
    remote_unpair.ble.connection_generation = 17;
    buddy_state_reduce(&state, &remote_unpair, 1000, &action);

    assert(state.confirmation == BUDDY_CONFIRM_FACTORY_RESET);
    assert(!state.confirmation_acknowledge);
    assert(action.type == BUDDY_ACTION_NONE);
}

static void test_ble_security_events_update_owned_state_and_clear_sensitive_prompt(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t connected = {.type = BUDDY_EVENT_BLE_CONNECTED};
    buddy_event_t passkey = {.type = BUDDY_EVENT_BLE_PASSKEY};
    buddy_event_t encrypted = {.type = BUDDY_EVENT_BLE_ENCRYPTION};
    buddy_event_t disconnected = {.type = BUDDY_EVENT_BLE_DISCONNECTED};
    buddy_event_t prompt = test_prompt_event("req-secret", "Bash", "secret", 0, 1);

    passkey.ble.passkey = 123456;
    encrypted.ble.secure = true;
    buddy_state_init(&state, NULL);
    buddy_state_reduce(&state, &connected, 1000, &action);
    assert(state.ble_connected);
    assert(!state.ble_encrypted);
    buddy_state_reduce(&state, &passkey, 1001, &action);
    assert(state.passkey_visible);
    assert(state.passkey == 123456);
    buddy_state_reduce(&state, &encrypted, 1002, &action);
    assert(state.ble_encrypted);
    assert(!state.passkey_visible);

    buddy_state_reduce(&state, &prompt, 1003, &action);
    buddy_state_reduce(&state, &disconnected, 1004, &action);
    assert(!state.ble_connected);
    assert(!state.ble_encrypted);
    assert(state.heartbeat_stale);
    assert(state.prompt.id[0] == '\0');
}

static void test_stale_security_mailbox_event_cannot_override_latest_link(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t connected = {.type = BUDDY_EVENT_BLE_CONNECTED};
    buddy_event_t passkey = {.type = BUDDY_EVENT_BLE_PASSKEY};
    buddy_event_t encrypted = {.type = BUDDY_EVENT_BLE_ENCRYPTION};

    connected.ble.connection_generation = 8;
    passkey.ble.connection_generation = 7;
    passkey.ble.passkey = 123456;
    encrypted.ble.connection_generation = 7;
    encrypted.ble.secure = true;
    buddy_state_init(&state, NULL);
    buddy_state_reduce(&state, &connected, 1000, &action);
    assert(state.ble_connection_generation == 8);

    buddy_state_reduce(&state, &passkey, 1001, &action);
    buddy_state_reduce(&state, &encrypted, 1002, &action);
    assert(!state.passkey_visible);
    assert(!state.ble_encrypted);

    passkey.ble.connection_generation = 8;
    buddy_state_reduce(&state, &passkey, 1003, &action);
    assert(state.passkey_visible);
}

static void test_new_link_generation_invalidates_sensitive_state_when_disconnect_was_coalesced(void)
{
    buddy_state_t state;
    buddy_action_t action = {0};
    buddy_event_t connected = {.type = BUDDY_EVENT_BLE_CONNECTED};
    buddy_event_t prompt = test_prompt_event("old-link", "Bash", "deploy", 1, 1);
    buddy_event_t unpair = {.type = BUDDY_EVENT_UNPAIR_CONFIRMATION};

    buddy_state_init(&state, NULL);
    connected.ble.connection_generation = 7;
    buddy_state_reduce(&state, &connected, 1000, &action);
    prompt.ble.connection_generation = 7;
    buddy_state_reduce(&state, &prompt, 1001, &action);

    connected.ble.connection_generation = 8;
    buddy_state_reduce(&state, &connected, 1002, &action);
    assert(state.prompt.id[0] == '\0');
    assert(!state.connected);
    assert(state.heartbeat_stale);

    unpair.ble.connection_generation = 8;
    buddy_state_reduce(&state, &unpair, 1003, &action);
    assert(state.confirmation == BUDDY_CONFIRM_UNPAIR);
    connected.ble.connection_generation = 9;
    buddy_state_reduce(&state, &connected, 1004, &action);
    assert(state.confirmation == BUDDY_CONFIRM_NONE);
    assert(!state.confirmation_pending);
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
    test_denial_locks_and_emits_exactly_once();
    test_permission_action_is_bound_to_the_prompt_connection();
    test_permission_success_moves_from_attempted_to_successful_state();
    test_permission_failure_is_visible_and_same_id_replay_stays_locked();
    test_heartbeat_prompt_snapshot_clears_or_preserves_approval_lock();
    test_ui_snapshot_runtime_indicators_default_off();
    test_absent_prompt_is_ignored();
    test_timeout_stale_prompt_is_ignored();
    test_standalone_prompt_refreshes_liveness_clock();
    test_disconnect_then_reconnect_does_not_restore_prompt();
    test_mismatched_observed_prompt_is_ignored();
    test_nonterminated_prompt_id_is_ignored();
    test_truncated_prompt_id_is_ignored();
    test_long_ok_opens_settings();
    test_protocol_command_events_refresh_the_display();
    test_status_request_does_not_overwrite_message();
    test_unpair_confirmation_survives_a_heartbeat();
    test_unpair_confirmation_ok_emits_explicit_action();
    test_unpair_confirmation_down_cancels_without_action();
    test_parsed_heartbeat_approval_serializes_permission();
    test_normal_navigation_and_approval_scroll_are_distinct();
    test_settings_actions_have_separate_confirmations();
    test_remote_unpair_confirmation_remembers_ack();
    test_remote_unpair_cannot_replace_a_local_confirmation();
    test_ble_security_events_update_owned_state_and_clear_sensitive_prompt();
    test_stale_security_mailbox_event_cannot_override_latest_link();
    test_new_link_generation_invalidates_sensitive_state_when_disconnect_was_coalesced();
    return 0;
}
