#include "buddy_state.h"

#include <stddef.h>
#include <string.h>

#define BUDDY_HEART_ANIMATION_MS 500
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
}

static buddy_character_t buddy_character_for(const buddy_state_t *state, uint64_t now_ms)
{
    bool has_prompt = state->prompt.id[0] != '\0' && !state->heartbeat_stale;

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
        buddy_invalidate_prompt(state);
    }
}

static void buddy_set_ui_refresh(buddy_action_t *action)
{
    if (action != NULL) {
        action->type = BUDDY_ACTION_UI_REFRESH;
    }
}

static void buddy_apply_heartbeat(buddy_state_t *state, const buddy_heartbeat_t *heartbeat,
                                  uint64_t now_ms, buddy_action_t *action)
{
    uint64_t level = heartbeat->tokens / BUDDY_TOKEN_CELEBRATION_STEP;

    state->heartbeat = *heartbeat;
    state->connected = heartbeat->connected;
    state->connection = heartbeat->connected ? BUDDY_CONNECTION_CONNECTED : BUDDY_CONNECTION_OFFLINE;
    state->heartbeat_stale = !heartbeat->connected;
    if (!heartbeat->connected) {
        buddy_invalidate_prompt(state);
    }
    state->last_heartbeat_ms = now_ms;
    state->running = heartbeat->running;
    state->tokens = heartbeat->tokens;
    buddy_copy(state->name, sizeof(state->name), heartbeat->name);
    buddy_copy(state->owner, sizeof(state->owner), heartbeat->owner);
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
                               buddy_action_t *action)
{
    if (!prompt->connected || !buddy_prompt_id_is_valid(prompt) ||
        strcmp(prompt->id, state->last_approved_prompt_id) == 0) {
        return;
    }

    state->prompt = *prompt;
    state->connected = true;
    state->connection = BUDDY_CONNECTION_CONNECTED;
    state->heartbeat_stale = false;
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

static void buddy_approve_prompt(buddy_state_t *state, const buddy_event_t *event,
                                 uint64_t now_ms, buddy_action_t *action)
{
    if (state->prompt.id[0] == '\0' || state->heartbeat_stale ||
        !buddy_prompt_id_is_valid(&state->prompt) ||
        !buddy_observed_prompt_matches(event, &state->prompt)) {
        return;
    }

    if (action != NULL) {
        action->type = BUDDY_ACTION_PERMISSION;
        buddy_copy(action->permission.id, sizeof(action->permission.id), state->prompt.id);
        buddy_copy(action->permission.tool, sizeof(action->permission.tool), state->prompt.tool);
        buddy_copy(action->permission.hint, sizeof(action->permission.hint), state->prompt.hint);
        action->permission.decision = BUDDY_PERMISSION_ONCE;
    }
    buddy_copy(state->last_approved_prompt_id, sizeof(state->last_approved_prompt_id), state->prompt.id);
    buddy_invalidate_prompt(state);
    state->temporary_character = BUDDY_CHARACTER_HEART;
    state->temporary_until_ms = now_ms + BUDDY_HEART_ANIMATION_MS;
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
        buddy_apply_heartbeat(state, &event->heartbeat, now_ms, action);
        break;
    case BUDDY_EVENT_PROMPT:
        buddy_apply_prompt(state, &event->prompt, action);
        break;
    case BUDDY_EVENT_TIME:
        buddy_copy(state->time, sizeof(state->time), event->command.value);
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
        }
        break;
    case BUDDY_EVENT_UNPAIR_CONFIRMATION:
        buddy_invalidate_prompt(state);
        state->confirmation_pending = true;
        state->connection = BUDDY_CONNECTION_CONFIRMING;
        buddy_set_ui_refresh(action);
        break;
    case BUDDY_EVENT_KEY_CLICK:
        if (state->confirmation_pending &&
            (event->key == BUDDY_KEY_OK || event->key == BUDDY_KEY_BACK)) {
            state->confirmation_pending = false;
            state->connection = state->connected ? BUDDY_CONNECTION_CONNECTED
                                                 : BUDDY_CONNECTION_OFFLINE;
            buddy_set_ui_refresh(action);
            break;
        }
        if (event->key == BUDDY_KEY_OK) {
            buddy_approve_prompt(state, event, now_ms, action);
        }
        break;
    case BUDDY_EVENT_KEY_LONG:
        if (event->key == BUDDY_KEY_OK) {
            state->page = BUDDY_PAGE_SETTINGS;
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
    snapshot->tokens = state->tokens;
    snapshot->heartbeat_stale = state->heartbeat_stale;
    snapshot->confirmation_pending = state->confirmation_pending;
    buddy_copy(snapshot->name, sizeof(snapshot->name), state->name);
    buddy_copy(snapshot->owner, sizeof(snapshot->owner), state->owner);
    buddy_copy(snapshot->time, sizeof(snapshot->time), state->time);
    buddy_copy(snapshot->message, sizeof(snapshot->message), state->message);
    buddy_copy_entries(snapshot->entries, state->entries);
    buddy_copy(snapshot->prompt_id, sizeof(snapshot->prompt_id), state->prompt.id);
    buddy_copy(snapshot->prompt_tool, sizeof(snapshot->prompt_tool), state->prompt.tool);
    buddy_copy(snapshot->prompt_hint, sizeof(snapshot->prompt_hint), state->prompt.hint);
}
