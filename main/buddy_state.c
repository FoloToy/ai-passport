#include "buddy_state.h"

#include <stddef.h>
#include <string.h>

#define BUDDY_HEART_ANIMATION_MS 5000
#define BUDDY_CELEBRATION_ANIMATION_MS 1500
#define BUDDY_HEARTBEAT_TIMEOUT_MS 30000
#define BUDDY_TOKEN_CELEBRATION_STEP 50000

static void buddy_copy(char *destination, size_t destination_size, const char *source)
{
    size_t length = 0;

    if (destination_size == 0) {
        return;
    }
    if (source != NULL) {
        while (length + 1 < destination_size && source[length] != '\0') {
            ++length;
        }
        memcpy(destination, source, length);
    }
    destination[length] = '\0';
}

static void buddy_copy_entries(char destination[BUDDY_ENTRY_COUNT][BUDDY_ENTRY_MAX],
                               const char source[BUDDY_ENTRY_COUNT][BUDDY_ENTRY_MAX])
{
    unsigned index;

    for (index = 0; index < BUDDY_ENTRY_COUNT; ++index) {
        buddy_copy(destination[index], BUDDY_ENTRY_MAX, source[index]);
    }
}

static bool buddy_string_matches_length(const char *value, size_t value_size, size_t length)
{
    size_t index;

    if (length == 0 || length >= value_size) {
        return false;
    }
    for (index = 0; index < value_size; ++index) {
        if (value[index] == '\0') {
            return index == length;
        }
    }
    return false;
}

static bool buddy_prompt_id_is_valid(const buddy_prompt_t *prompt)
{
    return !prompt->id_truncated &&
           buddy_string_matches_length(prompt->id, sizeof(prompt->id), prompt->id_length);
}

static void buddy_invalidate_prompt(buddy_state_t *state)
{
    memset(&state->prompt, 0, sizeof(state->prompt));
    state->prompt_connection_generation = 0;
    state->approval_locked = false;
}

static void buddy_clear_logical_session(buddy_state_t *state)
{
    state->connected = false;
    state->connection = BUDDY_CONNECTION_OFFLINE;
    state->total = 0;
    state->running = 0;
    state->waiting = 0;
    state->tokens = 0;
    state->tokens_today = 0;
    state->message[0] = '\0';
    memset(state->entries, 0, sizeof(state->entries));
    state->heartbeat.total = 0;
    state->heartbeat.running = 0;
    state->heartbeat.waiting = 0;
    state->heartbeat.tokens = 0;
    state->heartbeat.tokens_today = 0;
    state->heartbeat.message[0] = '\0';
    memset(state->heartbeat.entries, 0, sizeof(state->heartbeat.entries));
    buddy_invalidate_prompt(state);
}

static buddy_character_t buddy_character_for(const buddy_state_t *state, uint64_t now_ms)
{
    bool has_prompt = state->prompt.id[0] != '\0' && !state->heartbeat_stale &&
                      !state->approval_locked;

    if (state->confirmation_pending || state->connection == BUDDY_CONNECTION_CONFIRMING) {
        return BUDDY_CHARACTER_CONFIRMATION;
    }
    if (state->connection == BUDDY_CONNECTION_PAIRING) {
        return BUDDY_CHARACTER_PAIRING;
    }
    if (has_prompt) {
        return BUDDY_CHARACTER_ATTENTION;
    }
    if (state->temporary_until_ms > now_ms) {
        return state->temporary_character;
    }
    if (state->running > 0) {
        return BUDDY_CHARACTER_BUSY;
    }
    if (state->connected && !state->heartbeat_stale) {
        return BUDDY_CHARACTER_IDLE;
    }
    return BUDDY_CHARACTER_SLEEP;
}

static void buddy_refresh_character(buddy_state_t *state, uint64_t now_ms)
{
    state->character = buddy_character_for(state, now_ms);
}

static void buddy_clear_stale_prompt(buddy_state_t *state, uint64_t now_ms)
{
    if (!state->heartbeat_stale && now_ms - state->last_heartbeat_ms >= BUDDY_HEARTBEAT_TIMEOUT_MS) {
        state->heartbeat_stale = true;
        buddy_clear_logical_session(state);
    }
}

static void buddy_set_ui_refresh(buddy_action_t *action)
{
    if (action != NULL) {
        action->type = BUDDY_ACTION_UI_REFRESH;
    }
}

static bool buddy_has_actionable_prompt(const buddy_state_t *state)
{
    return state->prompt.id[0] != '\0' && !state->heartbeat_stale &&
           !state->approval_locked && buddy_prompt_id_is_valid(&state->prompt);
}

static bool buddy_has_prompt(const buddy_state_t *state)
{
    return state->prompt.id[0] != '\0';
}

static void buddy_open_confirmation(buddy_state_t *state, buddy_confirmation_t confirmation,
                                    bool acknowledge, uint32_t connection_generation,
                                    buddy_action_t *action)
{
    buddy_invalidate_prompt(state);
    state->confirmation = confirmation;
    state->confirmation_pending = confirmation != BUDDY_CONFIRM_NONE;
    state->confirmation_acknowledge = acknowledge;
    state->confirmation_connection_generation = connection_generation;
    state->connection = BUDDY_CONNECTION_CONFIRMING;
    buddy_set_ui_refresh(action);
}

static void buddy_close_confirmation(buddy_state_t *state)
{
    state->confirmation = BUDDY_CONFIRM_NONE;
    state->confirmation_pending = false;
    state->confirmation_acknowledge = false;
    state->confirmation_connection_generation = 0;
    state->connection = state->connected ? BUDDY_CONNECTION_CONNECTED
                                         : BUDDY_CONNECTION_OFFLINE;
}

static void buddy_settings_click(buddy_state_t *state, buddy_key_t key,
                                 buddy_action_t *action)
{
    if (key == BUDDY_KEY_UP) {
        state->settings_selection =
            (buddy_settings_item_t)((state->settings_selection + BUDDY_SETTINGS_COUNT - 1) %
                                    BUDDY_SETTINGS_COUNT);
        buddy_set_ui_refresh(action);
        return;
    }
    if (key == BUDDY_KEY_DOWN) {
        state->settings_selection =
            (buddy_settings_item_t)((state->settings_selection + 1) % BUDDY_SETTINGS_COUNT);
        buddy_set_ui_refresh(action);
        return;
    }
    if (key != BUDDY_KEY_OK) {
        return;
    }

    switch (state->settings_selection) {
    case BUDDY_SETTINGS_BLE:
        state->settings.ble_enabled = !state->settings.ble_enabled;
        if (action != NULL) {
            action->type = BUDDY_ACTION_BLE_TOGGLE;
            action->ble_enabled = state->settings.ble_enabled;
        }
        break;
    case BUDDY_SETTINGS_UNPAIR:
        buddy_open_confirmation(state, BUDDY_CONFIRM_UNPAIR, false, 0, action);
        break;
    case BUDDY_SETTINGS_FACTORY_RESET:
        buddy_open_confirmation(state, BUDDY_CONFIRM_FACTORY_RESET, false, 0, action);
        break;
    case BUDDY_SETTINGS_BACK:
    case BUDDY_SETTINGS_COUNT:
        state->page = BUDDY_PAGE_HOME;
        buddy_set_ui_refresh(action);
        break;
    }
}

static void buddy_normal_click(buddy_state_t *state, buddy_key_t key,
                               buddy_action_t *action)
{
    if (state->page == BUDDY_PAGE_SETTINGS) {
        buddy_settings_click(state, key, action);
    } else if (key == BUDDY_KEY_UP || key == BUDDY_KEY_DOWN) {
        state->page = state->page == BUDDY_PAGE_HOME ? BUDDY_PAGE_TRANSCRIPT
                                                     : BUDDY_PAGE_HOME;
        buddy_set_ui_refresh(action);
    }
}

static bool buddy_prompt_ids_match(const buddy_prompt_t *left, const buddy_prompt_t *right)
{
    return left->id_length == right->id_length &&
           left->id_length > 0 &&
           memcmp(left->id, right->id, left->id_length) == 0;
}

static bool buddy_prompt_was_attempted(const buddy_state_t *state,
                                       const buddy_prompt_t *prompt)
{
    return buddy_string_matches_length(state->last_attempted_prompt_id,
                                       sizeof(state->last_attempted_prompt_id),
                                       prompt->id_length) &&
           memcmp(state->last_attempted_prompt_id, prompt->id, prompt->id_length) == 0;
}

static void buddy_apply_heartbeat(buddy_state_t *state, const buddy_heartbeat_t *heartbeat,
                                  uint32_t connection_generation, uint64_t now_ms,
                                  buddy_action_t *action)
{
    uint64_t level = heartbeat->tokens / BUDDY_TOKEN_CELEBRATION_STEP;

    state->heartbeat = *heartbeat;
    state->connected = heartbeat->connected;
    state->connection = heartbeat->connected ? BUDDY_CONNECTION_CONNECTED : BUDDY_CONNECTION_OFFLINE;
    state->heartbeat_stale = !heartbeat->connected;
    if (!heartbeat->connected) {
        buddy_invalidate_prompt(state);
    } else if (heartbeat->prompt.id[0] == '\0') {
        buddy_invalidate_prompt(state);
    } else if (!buddy_prompt_id_is_valid(&heartbeat->prompt)) {
        buddy_invalidate_prompt(state);
    } else if (state->prompt.id[0] != '\0' &&
               buddy_prompt_ids_match(&state->prompt, &heartbeat->prompt)) {
        state->prompt = heartbeat->prompt;
        state->prompt_connection_generation = connection_generation;
    } else if (buddy_prompt_was_attempted(state, &heartbeat->prompt)) {
        buddy_invalidate_prompt(state);
    } else {
        state->prompt = heartbeat->prompt;
        state->prompt_connection_generation = connection_generation;
        state->approval_locked = false;
        state->permission_delivery = BUDDY_PERMISSION_DELIVERY_NONE;
    }
    state->last_heartbeat_ms = now_ms;
    state->running = heartbeat->running;
    state->total = heartbeat->total;
    state->waiting = heartbeat->waiting;
    state->tokens = heartbeat->tokens;
    state->tokens_today = heartbeat->tokens_today;
    buddy_copy(state->message, sizeof(state->message), heartbeat->message);
    buddy_copy_entries(state->entries, heartbeat->entries);

    if (level > state->highest_celebrated_level) {
        state->highest_celebrated_level = level;
        state->settings.highest_celebrated_level = level;
        state->temporary_character = BUDDY_CHARACTER_CELEBRATE;
        state->temporary_until_ms = now_ms + BUDDY_CELEBRATION_ANIMATION_MS;
        if (action != NULL) {
            action->type = BUDDY_ACTION_SETTINGS;
            action->settings = state->settings;
        }
        return;
    }
    buddy_set_ui_refresh(action);
}

static void buddy_apply_prompt(buddy_state_t *state, const buddy_prompt_t *prompt,
                               uint32_t connection_generation, uint64_t now_ms,
                               buddy_action_t *action)
{
    if (!prompt->connected || !buddy_prompt_id_is_valid(prompt) ||
        buddy_prompt_was_attempted(state, prompt)) {
        return;
    }

    state->prompt = *prompt;
    state->prompt_connection_generation = connection_generation;
    state->approval_locked = false;
    state->permission_delivery = BUDDY_PERMISSION_DELIVERY_NONE;
    state->connected = true;
    state->connection = BUDDY_CONNECTION_CONNECTED;
    state->heartbeat_stale = false;
    state->last_heartbeat_ms = now_ms;
    state->running = prompt->running;
    buddy_set_ui_refresh(action);
}

static bool buddy_observed_prompt_matches(const buddy_event_t *event, const buddy_prompt_t *prompt)
{
    if (!event->has_observed_prompt_id) {
        return true;
    }
    if (event->observed_prompt_id_truncated ||
        !buddy_string_matches_length(event->observed_prompt_id,
                                     sizeof(event->observed_prompt_id),
                                     event->observed_prompt_id_length)) {
        return false;
    }
    return event->observed_prompt_id_length == prompt->id_length &&
           memcmp(event->observed_prompt_id, prompt->id, prompt->id_length) == 0;
}

static void buddy_decide_prompt(buddy_state_t *state, const buddy_event_t *event,
                                buddy_permission_decision_t decision,
                                buddy_action_t *action)
{
    if (state->approval_locked || state->prompt.id[0] == '\0' || state->heartbeat_stale ||
        !buddy_prompt_id_is_valid(&state->prompt) ||
        !buddy_observed_prompt_matches(event, &state->prompt)) {
        return;
    }

    if (action != NULL) {
        action->type = BUDDY_ACTION_PERMISSION;
        buddy_copy(action->permission.id, sizeof(action->permission.id), state->prompt.id);
        buddy_copy(action->permission.tool, sizeof(action->permission.tool), state->prompt.tool);
        buddy_copy(action->permission.hint, sizeof(action->permission.hint), state->prompt.hint);
        action->permission.decision = decision;
        action->permission.connection_generation = state->prompt_connection_generation;
    }
    buddy_copy(state->last_attempted_prompt_id, sizeof(state->last_attempted_prompt_id),
               state->prompt.id);
    state->approval_locked = true;
    state->permission_delivery = BUDDY_PERMISSION_DELIVERY_SENDING;
}

static void buddy_apply_permission_result(buddy_state_t *state,
                                          const buddy_permission_result_event_t *result,
                                          uint64_t now_ms, buddy_action_t *action)
{
    if (state->permission_delivery != BUDDY_PERMISSION_DELIVERY_SENDING ||
        !buddy_string_matches_length(result->id, sizeof(result->id), result->id_length) ||
        !buddy_string_matches_length(state->last_attempted_prompt_id,
                                     sizeof(state->last_attempted_prompt_id),
                                     result->id_length) ||
        memcmp(state->last_attempted_prompt_id, result->id, result->id_length) != 0) {
        return;
    }
    if (result->success) {
        buddy_copy(state->last_successful_decision_id,
                   sizeof(state->last_successful_decision_id), result->id);
        state->permission_delivery = BUDDY_PERMISSION_DELIVERY_SENT;
        if (result->decision == BUDDY_PERMISSION_ONCE) {
            state->temporary_character = BUDDY_CHARACTER_HEART;
            state->temporary_until_ms = now_ms + BUDDY_HEART_ANIMATION_MS;
        }
    } else {
        state->permission_delivery = BUDDY_PERMISSION_DELIVERY_FAILED;
    }
    buddy_set_ui_refresh(action);
}

void buddy_state_init(buddy_state_t *state, const buddy_settings_snapshot_t *settings)
{
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->connection = BUDDY_CONNECTION_OFFLINE;
    state->page = BUDDY_PAGE_HOME;
    state->heartbeat_stale = true;
    if (settings != NULL) {
        state->settings = *settings;
        state->highest_celebrated_level = settings->highest_celebrated_level;
        buddy_copy(state->name, sizeof(state->name), settings->name);
        buddy_copy(state->owner, sizeof(state->owner), settings->owner);
    }
    buddy_refresh_character(state, 0);
}

void buddy_state_reduce(buddy_state_t *state, const buddy_event_t *event,
                        uint64_t now_ms, buddy_action_t *action)
{
    if (action != NULL) {
        memset(action, 0, sizeof(*action));
    }
    if (state == NULL || event == NULL) {
        return;
    }

    buddy_clear_stale_prompt(state, now_ms);

    switch (event->type) {
    case BUDDY_EVENT_HEARTBEAT:
        buddy_apply_heartbeat(state, &event->heartbeat, event->ble.connection_generation,
                              now_ms, action);
        break;
    case BUDDY_EVENT_PROMPT:
        buddy_apply_prompt(state, &event->prompt, event->ble.connection_generation,
                           now_ms, action);
        break;
    case BUDDY_EVENT_TIME:
        state->epoch_seconds = event->time.epoch_seconds;
        state->timezone_offset_seconds = event->time.timezone_offset_seconds;
        buddy_set_ui_refresh(action);
        break;
    case BUDDY_EVENT_NAME:
        buddy_copy(state->name, sizeof(state->name), event->command.value);
        buddy_set_ui_refresh(action);
        break;
    case BUDDY_EVENT_OWNER:
        buddy_copy(state->owner, sizeof(state->owner), event->command.value);
        buddy_set_ui_refresh(action);
        break;
    case BUDDY_EVENT_STATUS:
        buddy_copy(state->message, sizeof(state->message), event->command.value);
        buddy_set_ui_refresh(action);
        break;
    case BUDDY_EVENT_STATUS_REQUEST:
        if (action != NULL) {
            action->type = BUDDY_ACTION_STATUS;
            action->connection_generation = event->ble.connection_generation;
        }
        break;
    case BUDDY_EVENT_UNPAIR_CONFIRMATION:
        if (state->confirmation == BUDDY_CONFIRM_NONE) {
            buddy_open_confirmation(state, BUDDY_CONFIRM_UNPAIR, true,
                                    event->ble.connection_generation, action);
        }
        break;
    case BUDDY_EVENT_BLE_CONNECTED:
        if (state->ble_connection_generation != event->ble.connection_generation) {
            buddy_invalidate_prompt(state);
            state->passkey_visible = false;
            state->connected = false;
            state->heartbeat_stale = true;
            if (state->confirmation_acknowledge) {
                buddy_close_confirmation(state);
            }
        }
        state->ble_connection_generation = event->ble.connection_generation;
        state->ble_connected = true;
        state->ble_encrypted = false;
        if (state->confirmation == BUDDY_CONFIRM_NONE) {
            state->connection = BUDDY_CONNECTION_PAIRING;
        }
        buddy_set_ui_refresh(action);
        break;
    case BUDDY_EVENT_BLE_DISCONNECTED:
        state->ble_connection_generation = event->ble.connection_generation;
        state->ble_connected = false;
        state->ble_encrypted = false;
        state->passkey_visible = false;
        state->connected = false;
        state->heartbeat_stale = true;
        buddy_clear_logical_session(state);
        if (state->confirmation_acknowledge) {
            buddy_close_confirmation(state);
        } else if (state->confirmation == BUDDY_CONFIRM_NONE) {
            state->connection = BUDDY_CONNECTION_OFFLINE;
        }
        buddy_set_ui_refresh(action);
        break;
    case BUDDY_EVENT_BLE_PASSKEY:
        if (event->ble.connection_generation != state->ble_connection_generation) {
            break;
        }
        state->passkey = event->ble.passkey;
        state->passkey_visible = true;
        state->connection = BUDDY_CONNECTION_PAIRING;
        buddy_set_ui_refresh(action);
        break;
    case BUDDY_EVENT_BLE_ENCRYPTION:
        if (event->ble.connection_generation != state->ble_connection_generation) {
            break;
        }
        state->ble_encrypted = event->ble.secure;
        if (event->ble.secure) {
            state->passkey_visible = false;
            if (state->confirmation == BUDDY_CONFIRM_NONE) {
                state->connection = state->connected && !state->heartbeat_stale
                                        ? BUDDY_CONNECTION_CONNECTED
                                        : BUDDY_CONNECTION_OFFLINE;
            }
        } else if (state->ble_connected && state->confirmation == BUDDY_CONFIRM_NONE) {
            state->connection = BUDDY_CONNECTION_PAIRING;
        }
        buddy_set_ui_refresh(action);
        break;
    case BUDDY_EVENT_BOND_DELETE_RESULT:
        buddy_copy(state->message, sizeof(state->message),
                   event->ble.success ? "Unpaired" : "Unpair failed");
        buddy_set_ui_refresh(action);
        break;
    case BUDDY_EVENT_PERMISSION_SEND_RESULT:
        buddy_apply_permission_result(state, &event->permission_result, now_ms, action);
        break;
    case BUDDY_EVENT_KEY_CLICK:
        if (state->confirmation != BUDDY_CONFIRM_NONE && event->key == BUDDY_KEY_OK) {
            buddy_confirmation_t confirmation = state->confirmation;
            bool acknowledge = state->confirmation_acknowledge;
            uint32_t connection_generation = state->confirmation_connection_generation;

            buddy_close_confirmation(state);
            if (action != NULL) {
                action->type = confirmation == BUDDY_CONFIRM_UNPAIR
                                   ? BUDDY_ACTION_UNPAIR_CONFIRMED
                                   : BUDDY_ACTION_FACTORY_RESET_CONFIRMED;
                action->confirmation_acknowledge = acknowledge;
                action->connection_generation = connection_generation;
            }
            break;
        }
        if (state->confirmation != BUDDY_CONFIRM_NONE && event->key == BUDDY_KEY_DOWN) {
            buddy_close_confirmation(state);
            buddy_set_ui_refresh(action);
            break;
        }
        if (state->confirmation != BUDDY_CONFIRM_NONE) {
            break;
        }
        if (buddy_has_actionable_prompt(state) && event->key == BUDDY_KEY_OK) {
            buddy_decide_prompt(state, event, BUDDY_PERMISSION_ONCE, action);
        } else if (buddy_has_actionable_prompt(state) && event->key == BUDDY_KEY_DOWN) {
            buddy_decide_prompt(state, event, BUDDY_PERMISSION_DENY, action);
        } else if (buddy_has_actionable_prompt(state) && event->key == BUDDY_KEY_UP) {
            if (action != NULL) {
                action->type = BUDDY_ACTION_UI_SCROLL;
                action->scroll_delta = -48;
            }
        } else if (!buddy_has_prompt(state)) {
            buddy_normal_click(state, event->key, action);
        }
        break;
    case BUDDY_EVENT_KEY_LONG:
        if (event->key == BUDDY_KEY_OK && state->confirmation == BUDDY_CONFIRM_NONE &&
            !buddy_has_prompt(state)) {
            state->page = BUDDY_PAGE_SETTINGS;
            state->settings_selection = BUDDY_SETTINGS_BLE;
            buddy_set_ui_refresh(action);
        }
        break;
    case BUDDY_EVENT_TICK:
        buddy_set_ui_refresh(action);
        break;
    case BUDDY_EVENT_NONE:
        break;
    }

    buddy_refresh_character(state, now_ms);
}

void buddy_state_snapshot(const buddy_state_t *state, buddy_ui_snapshot_t *snapshot)
{
    if (state == NULL || snapshot == NULL) {
        return;
    }

    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->connection = state->connection;
    snapshot->character = state->character;
    snapshot->page = state->page;
    snapshot->running = state->running;
    snapshot->total = state->total;
    snapshot->waiting = state->waiting;
    snapshot->tokens = state->tokens;
    snapshot->tokens_today = state->tokens_today;
    snapshot->epoch_seconds = state->epoch_seconds;
    snapshot->timezone_offset_seconds = state->timezone_offset_seconds;
    snapshot->heartbeat_stale = state->heartbeat_stale;
    snapshot->confirmation_pending = state->confirmation_pending;
    snapshot->confirmation = state->confirmation;
    snapshot->settings_selection = state->settings_selection;
    snapshot->approval_locked = state->approval_locked;
    snapshot->permission_delivery = state->permission_delivery;
    snapshot->ble_connected = state->ble_connected;
    snapshot->ble_encrypted = state->ble_encrypted;
    snapshot->ble_enabled = state->settings.ble_enabled;
    snapshot->battery_available = state->battery_available;
    snapshot->passkey_visible = state->passkey_visible;
    snapshot->prompt_connection_generation = state->prompt_connection_generation;
    snapshot->confirmation_connection_generation =
        state->confirmation_connection_generation;
    snapshot->passkey = state->passkey;
    snapshot->battery_percent = state->battery_percent;
    snapshot->battery_mv = state->battery_mv;
    buddy_copy(snapshot->name, sizeof(snapshot->name), state->name);
    buddy_copy(snapshot->owner, sizeof(snapshot->owner), state->owner);
    buddy_copy(snapshot->time, sizeof(snapshot->time), state->time);
    buddy_copy(snapshot->message, sizeof(snapshot->message), state->message);
    buddy_copy_entries(snapshot->entries, state->entries);
    buddy_copy(snapshot->prompt_id, sizeof(snapshot->prompt_id), state->prompt.id);
    buddy_copy(snapshot->prompt_tool, sizeof(snapshot->prompt_tool), state->prompt.tool);
    buddy_copy(snapshot->prompt_hint, sizeof(snapshot->prompt_hint), state->prompt.hint);
}
