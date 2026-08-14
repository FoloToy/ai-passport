# Repository Guidelines

## Project Structure & Module Organization

This repository is an ESP-IDF 5.5.3 Claude Hardware Buddy firmware for the ESP32-C3
FoloToy AI Passport. Keep the board support layer independent from the Buddy application:

- `components/bsp/include/`: public board APIs and the pin/configuration source of truth
  (`bsp_pins.h`).
- `components/bsp/src/`: display, LVGL, button, audio, battery, and shared-I2C drivers.
- `main/main.c`: FreeRTOS orchestration, event queues, BLE-to-state integration, and the
  LVGL lock boundary.
- `main/buddy_ble*.c/.h`: NimBLE Nordic UART transport, secure pairing, bond lifecycle,
  connection generations, and encrypted TX/RX handling.
- `main/buddy_line*.c/.h`: bounded newline-delimited input framing.
- `main/buddy_protocol*.c/.h`: bounded JSON parsing and response serialization.
- `main/buddy_state*.c/.h`: hardware-independent connection, page, approval, and settings
  state transitions.
- `main/buddy_app_logic*.c/.h`: hardware-independent application helpers and queue policy.
- `main/buddy_settings*.c/.h`: NVS-backed name, owner, BLE preference, and counters.
- `main/buddy_character*.c/.h`: the phase-one ASCII character renderer and state frames.
- `main/buddy_ui*.c/.h`: LVGL pages and rendering from immutable state snapshots.
- `tests/`: host-side C tests for line framing, protocol, state, BLE policy, settings,
  character, and application logic.
- `README.md`: build instructions, user controls, limitations, and the required board
  acceptance checklist.

Keep reusable hardware logic in `components/bsp`; keep protocol, state, persistence, and
UI behavior in the focused `main/buddy_*` modules. BLE callbacks must queue bounded events
and must not create or mutate LVGL objects directly.

## Build, Test, and Development Commands

Use ESP-IDF 5.5.3 and target ESP32-C3:

```bash
get_idf553                    # Enter the repository's ESP-IDF 5.5.3 environment
idf.py set-target esp32c3     # Configure a fresh checkout
idf.py build                  # Compile firmware and validate dependencies
idf.py flash monitor          # Flash the connected board and open logs
idf.py fullclean              # Remove generated build state when configuration is stale
```

Run all host tests from an environment with `IDF_PATH` set:

```bash
get_idf553
cmake -S tests -B build-host
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

Before submitting, run both the host test suite and a fresh ESP-IDF build. Inspect compiler
warnings and size output; do not treat an existing build directory as evidence of a clean
configuration.

## Coding Style & Naming Conventions

Write C using four-space indentation and K&R-style braces, following nearby files. Use
`snake_case` for functions and locals, `BSP_*` for public board constants, `BUDDY_*` for
public application constants, and `s_` for file-local state. Keep board APIs prefixed with
`bsp_` and Buddy APIs prefixed with `buddy_`. Prefer `static` for internal symbols. UI text
stays English; explanatory comments may be Chinese. Preserve comments documenting
hardware-specific register values, security boundaries, and memory constraints.

Keep protocol and state logic testable without ESP-IDF hardware. Copy external input into
fixed-size buffers, validate critical identifiers before acting on them, and fail closed
for malformed JSON, stale connection generations, expired approvals, failed sends, and
unknown commands. Do not log complete approval IDs or sensitive tool parameters.

## BLE, Security, and Resource Constraints

The transport is the public Claude Hardware Buddy Nordic UART protocol. RX, TX, and the TX
CCCD require an encrypted, authenticated, bonded LE Secure Connections link. Pairing uses a
displayed six-digit passkey; bond deletion and factory reset require an on-device
confirmation. `sec` status must only report true for the current secured connection.

Use bounded line, JSON, queue, and notification buffers. Preserve priority for connection,
security, and approval events when queues are full. Do not add Arduino or board-compatibility
layers, GIF/file-transfer dependencies, or large bitmap assets to phase one. The ESP32-C3
has no PSRAM; check internal RAM, LVGL buffers, BLE allocations, and application partition
headroom when changing resource sizes.

## Testing and Hardware Validation

Host tests must cover complete, fragmented, consecutive, malformed, and oversized protocol
lines; state transitions and the 30-second heartbeat timeout; approval-ID matching and
duplicate-decision locking; secure BLE generation/transport policy; settings persistence;
and serialization boundaries.

On real hardware, use Claude Desktop developer mode and verify `Claude-*` discovery,
six-digit secure pairing, encrypted status, bond-backed reconnect, device-side unpair,
state pages, approval once/deny behavior, settings confirmations, battery/status fields,
and safe behavior after disconnect or heartbeat timeout. Exercise at least 20
connect/disconnect cycles and a 30-minute connected soak while recording heap, watchdog,
allocation, and BLE errors. Mark unavailable checks `NOT RUN`; do not claim physical
acceptance from host tests or a firmware build alone. Follow the complete checklist in
`README.md`.

## Commit & Pull Request Guidelines

Use focused Conventional Commit subjects such as `feat(buddy): ...`, `fix(buddy): ...`,
`fix(bsp): ...`, and `docs: ...`. Keep changes scoped to one subsystem. Pull requests
should explain firmware behavior changes, list host/build commands and their results,
identify the board revision and hardware checks performed, and call out any BLE/security,
pin-map, display, memory, or compatibility impact.
