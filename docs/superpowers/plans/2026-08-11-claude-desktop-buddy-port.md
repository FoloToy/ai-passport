# Claude Desktop Buddy Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the FoloToy-Card hardware demo with a secure, native ESP-IDF Claude Desktop Buddy that displays session state and lets the user approve or deny permission prompts.

**Architecture:** Keep `components/bsp` unchanged and replace the demo application with focused C modules for pure state, JSON protocol, NVS settings, NimBLE NUS transport, ASCII character rendering, LVGL pages, and orchestration. BLE callbacks publish bounded events to the application task; only the LVGL task/lock holder mutates UI objects.

**Tech Stack:** ESP-IDF 5.5.3, ESP32-C3 NimBLE, FreeRTOS queues/timers, cJSON, NVS, LVGL 9.5, existing FoloToy-Card BSP, host CTest tests.

## Global Constraints

- Target ESP32-C3 with no PSRAM and a 4 MB-compatible image.
- Use ESP-IDF 5.5.3; do not add Arduino or M5StickCPlus compatibility layers.
- Preserve `components/bsp` as the board hardware source of truth.
- UI text is English; explanatory hardware comments may be Chinese.
- All received lines, strings, entry counts, queues, and TX payloads have compile-time bounds.
- BLE callbacks never call LVGL; only code holding `bsp_lvgl_lock()` may mutate LVGL objects.
- Never persist recent messages, tool hints, prompt IDs, or live session state.
- Never approve an absent, truncated, stale, or mismatched prompt ID.
- Do not implement GIF decoding, LittleFS character storage, or folder push in phase one.
- Preserve upstream license/attribution for any copied or adapted source or artwork.

## File Map

- `main/buddy_types.h`: fixed-size domain types, limits, event and action contracts.
- `main/buddy_state.h`, `main/buddy_state.c`: deterministic application reducer, timeout and key handling.
- `main/buddy_line.h`, `main/buddy_line.c`: bounded newline framing for fragmented BLE RX.
- `main/buddy_protocol.h`, `main/buddy_protocol.c`: cJSON parsing and response serialization.
- `main/buddy_settings.h`, `main/buddy_settings.c`: NVS-backed non-sensitive settings and counters.
- `main/buddy_ble.h`, `main/buddy_ble.c`: encrypted NimBLE Nordic UART Service and bond management.
- `main/buddy_character.h`, `main/buddy_character.c`: one ASCII character and seven-state frame selection.
- `main/buddy_ui.h`, `main/buddy_ui.c`: Buddy, transcript, status, approval, pairing, settings and confirmation views.
- `main/main.c`: BSP initialization, queues, application loop, button translation and module wiring.
- `main/CMakeLists.txt`: phase-one source list and ESP-IDF component dependencies.
- `sdkconfig.defaults`: BLE/NimBLE/security defaults and bounded connection resources.
- `tests/CMakeLists.txt`: host test build using ESP-IDF cJSON.
- `tests/test_buddy_state.c`: state, timeout and permission safety tests.
- `tests/test_buddy_line.c`: fragmented, multiple and oversized line tests.
- `tests/test_buddy_protocol.c`: JSON schema and serialization tests.
- `README.md`: new firmware behavior, pairing, controls, build and hardware acceptance.
- Delete `main/demo.h`, `main/demo_audio.c`, `main/demo_battery.c`, `main/demo_button.c`, `main/demo_display.c`, `main/ui_pixel.c`, `main/ui_pixel.h`, `main/ui_pixel_math.c`, `main/ui_pixel_math.h`, and `tests/test_ui_pixel_math.c` after replacements pass.

---

### Task 1: Pure Domain Model and State Reducer

**Files:**
- Create: `main/buddy_types.h`
- Create: `main/buddy_state.h`
- Create: `main/buddy_state.c`
- Create: `tests/CMakeLists.txt`
- Create: `tests/test_buddy_state.c`

**Interfaces:**
- Produces: `buddy_state_init(buddy_state_t *, const buddy_settings_snapshot_t *)`, `buddy_state_reduce(buddy_state_t *, const buddy_event_t *, uint64_t now_ms, buddy_action_t *)`, and `buddy_state_snapshot(const buddy_state_t *, buddy_ui_snapshot_t *)`.
- Produces limits: `BUDDY_NAME_MAX=32`, `BUDDY_OWNER_MAX=32`, `BUDDY_MESSAGE_MAX=160`, `BUDDY_ENTRY_MAX=96`, `BUDDY_ENTRY_COUNT=4`, `BUDDY_PROMPT_ID_MAX=96`, `BUDDY_TOOL_MAX=48`, `BUDDY_HINT_MAX=320`, `BUDDY_JSON_LINE_MAX=4096`.
- Consumes: no ESP-IDF or LVGL APIs, so this task remains host-testable.

- [ ] **Step 1: Write the failing state tests**

Create table-driven assertions for offline initialization, heartbeat mapping, state priority, timeout clearing, each crossed 50,000-token boundary producing one celebration, fast approval heart animation, repeated approval locking, new prompt unlocking, and long-OK settings navigation. The core safety case must be explicit:

```c
buddy_state_t state;
buddy_action_t action = {0};
buddy_state_init(&state, NULL);

buddy_event_t prompt = test_prompt_event("req-1", "Bash", "git push", 1, 1);
buddy_state_reduce(&state, &prompt, 1000, &action);
assert(state.character == BUDDY_CHARACTER_ATTENTION);

buddy_event_t approve = {.type = BUDDY_EVENT_KEY_CLICK, .key = BUDDY_KEY_OK};
buddy_state_reduce(&state, &approve, 2000, &action);
assert(action.type == BUDDY_ACTION_PERMISSION);
assert(strcmp(action.permission.id, "req-1") == 0);
assert(action.permission.decision == BUDDY_PERMISSION_ONCE);

memset(&action, 0, sizeof(action));
buddy_state_reduce(&state, &approve, 2100, &action);
assert(action.type == BUDDY_ACTION_NONE);
```

- [ ] **Step 2: Add the host test harness and verify failure**

Use CMake/CTest with strict warnings and no embedded headers:

```cmake
cmake_minimum_required(VERSION 3.16)
project(buddy_host_tests C)
enable_testing()
add_executable(test_buddy_state test_buddy_state.c ../main/buddy_state.c)
target_include_directories(test_buddy_state PRIVATE ../main)
target_compile_options(test_buddy_state PRIVATE -Wall -Wextra -Werror)
add_test(NAME buddy_state COMMAND test_buddy_state)
```

Run: `cmake -S tests -B build-host && cmake --build build-host && ctest --test-dir build-host --output-on-failure`

Expected: FAIL because `buddy_types.h` and reducer symbols do not exist.

- [ ] **Step 3: Define bounded types and implement the minimal reducer**

Define enums for connection, character, page, event, action and decision. Use fixed arrays in `buddy_heartbeat_t`, `buddy_prompt_t`, `buddy_state_t`, `buddy_ui_snapshot_t`, and `buddy_action_t`. Implement priority in this exact order: confirmation/pairing → approval → temporary animation → busy → idle → sleep. Treat `now_ms - last_heartbeat_ms >= 30000` as timeout and clear the prompt before emitting a UI refresh. Trigger one celebration when cumulative `tokens / 50000` advances, persist the highest celebrated level through the settings action, and do not replay old levels after reboot.

```c
bool has_prompt = state->prompt.id[0] != '\0' && !state->heartbeat_stale;
if (has_prompt) return BUDDY_CHARACTER_ATTENTION;
if (state->temporary_until_ms > now_ms) return state->temporary_character;
if (state->running > 0) return BUDDY_CHARACTER_BUSY;
if (state->connected && !state->heartbeat_stale) return BUDDY_CHARACTER_IDLE;
return BUDDY_CHARACTER_SLEEP;
```

- [ ] **Step 4: Run the reducer tests**

Run: `cmake --build build-host && ctest --test-dir build-host -R buddy_state --output-on-failure`

Expected: PASS, including stale/mismatched prompt cases producing `BUDDY_ACTION_NONE`.

- [ ] **Step 5: Commit the domain model**

```bash
git add main/buddy_types.h main/buddy_state.h main/buddy_state.c tests/CMakeLists.txt tests/test_buddy_state.c
git commit -m "feat(core): add Buddy state reducer"
```

### Task 2: Bounded BLE Line Framing and JSON Protocol

**Files:**
- Create: `main/buddy_line.h`
- Create: `main/buddy_line.c`
- Create: `main/buddy_protocol.h`
- Create: `main/buddy_protocol.c`
- Create: `tests/test_buddy_line.c`
- Create: `tests/test_buddy_protocol.c`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `buddy_event_t`, `buddy_heartbeat_t`, limits and permission decisions from `buddy_types.h`.
- Produces: `buddy_line_init()`, `buddy_line_push(..., buddy_line_callback_t, void *)`, `buddy_protocol_parse(const char *, size_t, buddy_event_t *)`, `buddy_protocol_permission_json(...)`, `buddy_protocol_ack_json(...)`, and `buddy_protocol_status_json(...)`.
- `buddy_line_push` calls back once per complete line without the newline; it returns `BUDDY_LINE_OVERFLOW` after discarding an oversized line through its next newline.

- [ ] **Step 1: Write failing line-framing tests**

Cover a line split across three writes, two lines in one write, CRLF trimming, an exact-limit line, and recovery after overflow:

```c
buddy_line_buffer_t rx;
buddy_line_init(&rx);
buddy_line_push(&rx, (const uint8_t *)"{\"a\":", 5, capture_line, &capture);
buddy_line_push(&rx, (const uint8_t *)"1}\n{\"b\":2}\n", 13, capture_line, &capture);
assert(capture.count == 2);
assert(strcmp(capture.lines[0], "{\"a\":1}") == 0);
```

- [ ] **Step 2: Build and confirm framing tests fail**

Run: `cmake --build build-host`

Expected: FAIL because the line buffer symbols are missing.

- [ ] **Step 3: Implement the fixed-capacity line buffer**

Store `char data[BUDDY_JSON_LINE_MAX + 1]`, `size_t length`, and `bool discarding`. Never allocate. When overflow begins, reset `length`, set `discarding`, and ignore bytes through `\n`; the next byte begins a fresh line.

- [ ] **Step 4: Add failing protocol tests**

Test the documented heartbeat, optional prompt, `time`, `owner`, `name`, `status`, `unpair`, unsupported `char_begin`, unknown command, malformed JSON, non-object JSON, oversized prompt ID, bounded display-string truncation, and serializers. Include exact permission output:

```c
char json[192];
assert(buddy_protocol_permission_json(json, sizeof(json), "req_abc123",
                                      BUDDY_PERMISSION_ONCE) > 0);
assert(strcmp(json,
    "{\"cmd\":\"permission\",\"id\":\"req_abc123\",\"decision\":\"once\"}\n") == 0);
```

- [ ] **Step 5: Implement cJSON parsing and bounded serialization**

Use `cJSON_ParseWithLengthOpts` and require the parser end pointer to consume the complete line. Copy display fields with a helper that always NUL-terminates and marks truncation; reject a prompt if `id` is missing, empty, not a string, or does not fit. Return `BUDDY_EVENT_UNSUPPORTED_COMMAND` for folder-push commands so the application can ack `ok:false`.

- [ ] **Step 6: Link host cJSON and run all protocol tests**

In `tests/CMakeLists.txt`, require `$IDF_PATH` and compile `${IDF_PATH}/components/json/cJSON/cJSON.c` with `${IDF_PATH}/components/json/cJSON` in the include path.

Run: `bash -lc 'get_idf553 && cmake -S tests -B build-host && cmake --build build-host && ctest --test-dir build-host --output-on-failure'`

Expected: PASS for `buddy_state`, `buddy_line`, and `buddy_protocol`.

- [ ] **Step 7: Commit protocol support**

```bash
git add main/buddy_line.* main/buddy_protocol.* tests/CMakeLists.txt tests/test_buddy_line.c tests/test_buddy_protocol.c
git commit -m "feat(protocol): implement Hardware Buddy JSON framing"
```

### Task 3: NVS Settings and Rate-Limited Statistics

**Files:**
- Create: `main/buddy_settings.h`
- Create: `main/buddy_settings.c`
- Modify: `main/buddy_types.h`
- Modify: `main/CMakeLists.txt`

**Interfaces:**
- Consumes: `buddy_settings_snapshot_t` and permission result events.
- Produces: `buddy_settings_init()`, `buddy_settings_load()`, `buddy_settings_set_name()`, `buddy_settings_set_owner()`, `buddy_settings_set_ble_enabled()`, `buddy_settings_record_permission()`, `buddy_settings_flush(bool force)`, and `buddy_settings_factory_reset()`.
- NVS namespace is `buddy`; keys are `name`, `owner`, `ble`, `approve`, `deny`, and `level`.

- [ ] **Step 1: Add an injectable storage backend and failing unit test**

Define internal function pointers for get/set/erase/commit so a small fake backend can assert that five rapid approvals update RAM but call commit only once on forced flush:

```c
for (int i = 0; i < 5; ++i) {
    buddy_settings_record_permission(BUDDY_PERMISSION_ONCE);
}
assert(fake.commit_count == 0);
assert(buddy_settings_flush(true) == ESP_OK);
assert(fake.commit_count == 1);
```

Add this executable to CTest with `-DESP_PLATFORM` disabled and a local minimal `esp_err.h` test shim only for error constants.

- [ ] **Step 2: Run the settings test and verify failure**

Run: `cmake --build build-host && ctest --test-dir build-host -R buddy_settings --output-on-failure`

Expected: FAIL because settings APIs do not exist.

- [ ] **Step 3: Implement settings validation and NVS adapter**

Use defaults `Claude-<MAC suffix>` at orchestration time, empty owner, and BLE enabled. Names must be valid UTF-8 after JSON parsing, non-empty, and at most `BUDDY_NAME_MAX` bytes. Increment counters in RAM and flush at most once per 60 seconds or on orderly reset; `factory_reset` erases only namespace `buddy`, not unrelated NVS namespaces.

- [ ] **Step 4: Run host tests and an ESP-IDF compile**

Run: `bash -lc 'get_idf553 && cmake --build build-host && ctest --test-dir build-host --output-on-failure && idf.py reconfigure && idf.py build'`

Expected: all host tests PASS and firmware compiles with `nvs_flash` linked.

- [ ] **Step 5: Commit persistence**

```bash
git add main/buddy_settings.* main/buddy_types.h main/CMakeLists.txt tests
git commit -m "feat(settings): persist Buddy preferences and stats"
```

### Task 4: Secure NimBLE Nordic UART Service

**Files:**
- Create: `main/buddy_ble.h`
- Create: `main/buddy_ble.c`
- Modify: `main/CMakeLists.txt`
- Modify: `sdkconfig.defaults`

**Interfaces:**
- Consumes: a `buddy_ble_event_cb_t` callback for connection, disconnection, passkey, encryption and complete RX line events.
- Produces: `buddy_ble_init(const buddy_ble_config_t *)`, `buddy_ble_start()`, `buddy_ble_stop()`, `buddy_ble_send(const char *, size_t)`, `buddy_ble_is_connected()`, `buddy_ble_is_encrypted()`, and `buddy_ble_delete_bonds()`.
- Uses `buddy_line_buffer_t` per connection and sends TX notifications in negotiated-MTU-sized fragments.

- [ ] **Step 1: Add a failing compile-time UUID and policy test**

Expose UUID byte constants and a small pure helper `buddy_ble_tx_fragment_size(uint16_t mtu)`; test UUID values and `mtu - 3`, clamped to the TX scratch-buffer capacity:

```c
assert(BUDDY_NUS_SERVICE_UUID16_FIRST == 0x0001);
assert(buddy_ble_tx_fragment_size(23) == 20);
assert(buddy_ble_tx_fragment_size(517) <= BUDDY_BLE_TX_CHUNK_MAX);
```

- [ ] **Step 2: Enable NimBLE and confirm the build reaches missing BLE symbols**

Add these defaults:

```text
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_BT_NIMBLE_ROLE_PERIPHERAL=y
CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1
CONFIG_BT_NIMBLE_SM_LEGACY=n
CONFIG_BT_NIMBLE_SM_SC=y
CONFIG_BT_NIMBLE_NVS_PERSIST=y
```

Run: `bash -lc 'get_idf553 && idf.py fullclean && idf.py set-target esp32c3 && idf.py build'`

Expected: FAIL until `buddy_ble.c` supplies the declared implementation.

- [ ] **Step 3: Implement encrypted NUS characteristics and advertising**

Register the standard 128-bit service/RX/TX UUIDs. Require encrypted writes on RX and encrypted reads/notifications plus encrypted CCCD access on TX. Advertise the complete NUS UUID and `Claude-<MAC suffix>` name. Limit to one connection and restart advertising after disconnect.

- [ ] **Step 4: Implement Secure Connections callbacks and bond deletion**

Configure DisplayOnly IO, bonding, MITM and SC. Publish the six-digit passkey and encryption result to the application callback. On `buddy_ble_delete_bonds()`, terminate the active connection, delete all peer bonds through NimBLE store APIs, and restart advertising. Do not log passkeys after the pairing event or any received JSON payload.

- [ ] **Step 5: Implement bounded RX and fragmented TX**

Feed RX mbuf chains into `buddy_line_push`; publish only complete lines. Serialize sends with a mutex, require active connection plus encryption, and fragment notifications by negotiated ATT MTU. Return `ESP_ERR_INVALID_STATE` on an unencrypted link.

- [ ] **Step 6: Build and inspect security configuration**

Run: `bash -lc 'get_idf553 && idf.py build && grep -E "CONFIG_BT_(ENABLED|NIMBLE_ENABLED|NIMBLE_SM_SC|NIMBLE_NVS_PERSIST)=y" sdkconfig'`

Expected: clean build and all four settings present.

- [ ] **Step 7: Commit BLE transport**

```bash
git add main/buddy_ble.* main/CMakeLists.txt sdkconfig.defaults
git commit -m "feat(ble): add secure Hardware Buddy NUS transport"
```

### Task 5: ASCII Character and LVGL Pages

**Files:**
- Create: `main/buddy_character.h`
- Create: `main/buddy_character.c`
- Create: `main/buddy_ui.h`
- Create: `main/buddy_ui.c`
- Create: `tests/test_buddy_character.c`
- Modify: `tests/CMakeLists.txt`
- Modify: `main/CMakeLists.txt`

**Interfaces:**
- Consumes: `buddy_ui_snapshot_t` only; UI does not read mutable state or BLE globals.
- Produces: `buddy_character_frame(state, elapsed_ms)`, `buddy_ui_init()`, `buddy_ui_render(const buddy_ui_snapshot_t *)`, `buddy_ui_show_passkey(uint32_t)`, and `buddy_ui_tick(uint64_t)`.
- All `buddy_ui_*` entry points require the caller to hold `bsp_lvgl_lock()`.

- [ ] **Step 1: Write failing deterministic character-frame tests**

For each of seven states, assert frame 0 at time 0, valid wrap at the animation period, and non-empty ASCII. Verify attention alternates visibly while sleep changes slowly:

```c
const char *a = buddy_character_frame(BUDDY_CHARACTER_ATTENTION, 0);
const char *b = buddy_character_frame(BUDDY_CHARACTER_ATTENTION, 500);
assert(a && b && strcmp(a, b) != 0);
assert(buddy_character_frame(BUDDY_CHARACTER_SLEEP, 0) != NULL);
```

- [ ] **Step 2: Run the test and verify failure**

Run: `cmake --build build-host && ctest --test-dir build-host -R buddy_character --output-on-failure`

Expected: FAIL because character frames are missing.

- [ ] **Step 3: Implement one original ASCII character**

Create compact original frames for sleep, idle, busy, attention, celebrate, dizzy and heart. Do not copy upstream artwork unless its license header and attribution are added in the same commit. Keep frame strings in flash as `static const char[]`.

- [ ] **Step 4: Build the 240×320 UI hierarchy**

Create one screen with top status bar, center monospace-like character label, bottom summary and hint labels. Create lazy pages for transcript, status, settings, approval, passkey and confirmation. Use Montserrat 14/20 already enabled; use text wrapping and fixed-height scroll containers for hints. `buddy_ui_render` switches visibility based solely on the snapshot.

- [ ] **Step 5: Add approval and confirmation visual rules**

Approval must show tool, hint, `OK Approve once`, and `DOWN Deny`; locked submissions replace controls with `Sending...`. Unpair/factory reset use a separate confirmation view with `OK Confirm` and `DOWN Cancel`. Never place an approval action on a settings confirmation page.

- [ ] **Step 6: Run tests and firmware build**

Run: `bash -lc 'get_idf553 && cmake --build build-host && ctest --test-dir build-host --output-on-failure && idf.py build'`

Expected: all tests PASS and LVGL sources compile without increasing `CONFIG_LV_MEM_SIZE_KILOBYTES` above 24.

- [ ] **Step 7: Commit UI and character rendering**

```bash
git add main/buddy_character.* main/buddy_ui.* main/CMakeLists.txt tests
git commit -m "feat(ui): add Claude Buddy pages and character"
```

### Task 6: Application Orchestration, Buttons, Status and Commands

**Files:**
- Replace: `main/main.c`
- Modify: `main/buddy_state.c`
- Modify: `main/buddy_protocol.c`
- Modify: `main/buddy_settings.c`
- Modify: `main/CMakeLists.txt`
- Modify: `tests/test_buddy_state.c`
- Modify: `tests/test_buddy_protocol.c`

**Interfaces:**
- Consumes all phase-one module APIs.
- Produces the running firmware event loop and `status`, `name`, `owner`, `time`, `unpair`, BLE toggle, factory reset and permission command behavior.

- [ ] **Step 1: Add failing end-to-end reducer/action tests**

Drive parsed heartbeat → approval click → permission JSON and parsed commands → action/ack. Assert `unpair` produces a confirmation action rather than immediate deletion, unsupported folder push produces `{"ack":"char_begin","ok":false,...}`, and status omits unavailable battery fields instead of inventing values.

- [ ] **Step 2: Run host tests and verify the missing orchestration behavior fails**

Run: `bash -lc 'get_idf553 && cmake --build build-host && ctest --test-dir build-host --output-on-failure'`

Expected: FAIL on the newly asserted action mappings.

- [ ] **Step 3: Replace demo startup with production initialization**

Initialize in order: NVS (erase/retry only for `ESP_ERR_NVS_NO_FREE_PAGES` or `ESP_ERR_NVS_NEW_VERSION_FOUND`), I2C, display/LVGL, battery, settings, application queue/task, buttons, then BLE when enabled. Display failure remains fatal; battery failure marks battery unavailable; audio is not initialized.

```c
static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *ctx)
{
    buddy_event_t event;
    if (buddy_translate_key(btn, ev, &event)) {
        xQueueSend(s_event_queue, &event, 0);
    }
}
```

- [ ] **Step 4: Add the single-owner application loop**

The application task owns `buddy_state_t`. It consumes queue events, invokes the reducer, executes at most one emitted action, snapshots state, and renders under `bsp_lvgl_lock()`. Use a 100 ms receive timeout to run heartbeat expiry, animation ticks, battery sampling, settings flush and UI refresh without separate state-mutating timers.

- [ ] **Step 5: Wire commands and safe acknowledgements**

Implement `status` from actual `esp_timer_get_time`, `esp_get_free_heap_size`, battery readings, settings counters and current encryption state. Apply validated `name`/`owner`, then ack. Route `unpair` to confirmation; after confirmation, ack if connected, delete bonds and clear transient state. Folder-push commands return `ok:false,error:"unsupported in phase 1"`.

- [ ] **Step 6: Wire exact button semantics**

Normal pages: UP/DOWN navigate or scroll; OK long opens settings. Approval: UP scrolls, DOWN click denies, OK click approves once. Confirmation: OK click confirms, DOWN click cancels. Ignore DOUBLE and PRESS for decisions so a physical gesture cannot generate two permissions.

- [ ] **Step 7: Run all host tests and firmware build**

Run: `bash -lc 'get_idf553 && cmake --build build-host && ctest --test-dir build-host --output-on-failure && idf.py build'`

Expected: all host tests PASS and the firmware links with no undefined module symbols.

- [ ] **Step 8: Commit the integrated application**

```bash
git add main tests
git commit -m "feat(app): integrate Claude Buddy firmware flow"
```

### Task 7: Remove Demo Logic and Update Documentation

**Files:**
- Delete: `main/demo.h`
- Delete: `main/demo_audio.c`
- Delete: `main/demo_battery.c`
- Delete: `main/demo_button.c`
- Delete: `main/demo_display.c`
- Delete: `main/ui_pixel.c`
- Delete: `main/ui_pixel.h`
- Delete: `main/ui_pixel_math.c`
- Delete: `main/ui_pixel_math.h`
- Delete: `tests/test_ui_pixel_math.c`
- Modify: `main/CMakeLists.txt`
- Modify: `README.md`
- Create: `NOTICE`

**Interfaces:**
- Consumes: the now-complete Buddy application.
- Produces: a repository with no stale demo entry points and accurate user/build/acceptance documentation.

- [ ] **Step 1: Prove the new source list no longer needs demo files**

Remove demo/UI files from `main/CMakeLists.txt` first, then run:

Run: `bash -lc 'get_idf553 && idf.py build'`

Expected: PASS before deleting files, proving no hidden references remain.

- [ ] **Step 2: Delete obsolete demo sources**

Use `apply_patch` deletions for the listed files. Confirm no reference remains:

Run: `rg -n 'demo_|ui_pixel' main tests CMakeLists.txt README.md`

Expected: no code/build references; documentation history references may be rewritten rather than retained.

- [ ] **Step 3: Rewrite README for Claude Buddy**

Document developer-mode pairing, `Claude-*` discovery, six-digit secure pairing, automatic reconnection, page controls, approval controls, settings confirmations, ESP-IDF 5.5.3 build/flash commands, phase-one limitations, and the complete board acceptance checklist. State that the Hardware Buddy API is experimental and requires Claude Desktop developer mode.

- [ ] **Step 4: Add attribution and license notice**

`NOTICE` must identify protocol compatibility with Anthropic's `claude-desktop-buddy`, link the upstream repository, state whether any source/art was adapted, and reproduce any notice required by the upstream license. If no source or art was copied, say the implementation was written against the public wire-protocol reference and keep the repository's existing license situation unchanged.

- [ ] **Step 5: Run stale-reference, host-test and build checks**

Run: `bash -lc 'get_idf553 && ! rg -n "demo_|ui_pixel" main tests main/CMakeLists.txt && cmake --build build-host && ctest --test-dir build-host --output-on-failure && idf.py build'`

Expected: no stale references, all tests PASS, firmware build PASS.

- [ ] **Step 6: Commit cleanup and docs**

```bash
git add -A main tests README.md NOTICE
git commit -m "docs: replace BSP demo documentation with Claude Buddy"
```

### Task 8: Clean Build, Resource Audit and Hardware Acceptance

**Files:**
- Modify: `README.md` only if observed steps or limitations differ from documented behavior.
- Create: `docs/validation/2026-08-11-claude-buddy-phase1.md`

**Interfaces:**
- Consumes: complete phase-one firmware and a FoloToy-Card plus Claude Desktop in developer mode.
- Produces: reproducible build/resource evidence and an explicit hardware result record; does not claim unperformed checks passed.

- [ ] **Step 1: Run a clean host and firmware build**

Run:

```bash
bash -lc 'get_idf553 && cmake -S tests -B build-host && cmake --build build-host --clean-first && ctest --test-dir build-host --output-on-failure && idf.py fullclean && idf.py set-target esp32c3 && idf.py build'
```

Expected: every host test PASS and `idf.py build` exits 0 with no project-source warnings.

- [ ] **Step 2: Record Flash and RAM evidence**

Run: `bash -lc 'get_idf553 && idf.py size && idf.py size-components'`

Record application image size, free partition space, static DRAM/IRAM, and the largest components in the validation document. Confirm LVGL remains at 24 KB and no PSRAM setting is enabled.

- [ ] **Step 3: Flash and validate startup**

Run: `bash -lc 'get_idf553 && idf.py flash monitor'`

Observe successful NVS/BSP/NimBLE initialization, `Claude-*` advertising, Buddy sleep screen, battery state or explicit unavailable indication, and no reboot loop. Exit monitor with Ctrl-].

- [ ] **Step 4: Validate secure pairing and reconnect**

Enable Claude Desktop developer mode, open Hardware Buddy, select the device, compare and complete the six-digit pairing, verify encrypted status, reboot the card, and verify automatic reconnection without a new passkey. Use device-side Unpair and confirm that the next connection requires a fresh passkey.

- [ ] **Step 5: Validate live states and permission safety**

Exercise zero-session sleep/idle, an active session busy state, recent entries, a benign approval request, approve-once, a second request denied, rapid repeated button presses, disconnect during an approval, and a 30-second heartbeat timeout. Confirm only one response per prompt and no response after disconnect/timeout.

- [ ] **Step 6: Run a connection soak and record heap**

Repeat connect/disconnect at least 20 times and leave the device connected for at least 30 minutes while sampling status heap. Record initial/minimum/final free heap and any watchdog, allocation or BLE errors. A monotonic decline across reconnect cycles blocks completion.

- [ ] **Step 7: Finalize validation record and commit**

Mark every item `PASS`, `FAIL`, or `NOT RUN` with observed evidence; never convert unavailable hardware checks to PASS. If documentation required correction, include it.

```bash
git add docs/validation/2026-08-11-claude-buddy-phase1.md README.md
git commit -m "test: record Claude Buddy phase one validation"
```

## Completion Criteria

- All host tests pass from a fresh `build-host` directory.
- ESP-IDF 5.5.3 produces a clean ESP32-C3 build after `fullclean` and `set-target`.
- No deleted demo/UI symbol remains under `main/` or `tests/`.
- BLE service UUIDs, encrypted characteristics, passkey pairing, bonds and unpair behavior match the protocol design.
- Permission decisions are exactly once per current prompt ID and fail closed.
- Resource evidence confirms operation without PSRAM and without increasing the LVGL pool above 24 KB.
- Hardware checks are reported honestly as PASS, FAIL or NOT RUN in the validation record.
