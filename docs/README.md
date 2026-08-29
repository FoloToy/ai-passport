# FoloToy AI Passport

English | [简体中文](README.zh_CN.md)

FoloToy AI Passport is open wearable AI hardware. This repository is the development baseline for the device. It keeps the **hardware facts, stable interfaces, resource boundaries, reference implementations, and validation methods** needed to build applications in one place.

The repository is organized around the following principles:

- `main` is the smallest complete runnable baseline and an executable description of the current hardware capabilities.
- `components/bsp` isolates board-level details and exposes stable APIs to applications.
- `demo/*` branches show different paths from a product requirement to a working implementation.
- Development conventions for AI assistants live in [`AGENTS.md`](../AGENTS.md) and [`docs/development/agent-guide.md`](development/agent-guide.md); the complete hardware context and troubleshooting knowledge is in [`docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md`](hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md).
- Build results and physical-device results are reported separately. A successful build must never be presented as successful hardware validation.

## Hardware capability contract

The table below describes the application capabilities implemented by the current `main` branch. It is not a list of everything that might be possible according to the chip datasheet.

| Capability | Confirmed implementation | Application interface | Boundaries that must be respected |
| --- | --- | --- | --- |
| Display | ST7789P3, 240 × 320 portrait RGB565, SPI2 at 40 MHz; LEDC backlight | `bsp_display_*`, `bsp_lvgl_*` | The ESP32-C3 has no PSRAM; the current design uses a small single DMA buffer; the BSP exposes no LCD MISO, touch, or TE interface |
| Input | `UP`, `DOWN`, and `OK` share an ADC resistor ladder on GPIO0 | `bsp_button_init()`, `bsp_button_read_mv()` | Callbacks run in the button component task and must not block; do not create a second ADC1 unit |
| Audio | ES8311 with full-duplex PCM over I2S0, supporting playback and microphone capture | `bsp_audio_*` | PCM reads and writes block and belong in a worker task; format changes must retain the BSP close/open sequence |
| Battery | CW2017 state-of-charge and voltage readings | `bsp_battery_*` | This capability is optional at runtime; accuracy depends on the cell and battery profile and is not equivalent to a calibrated result |
| Wi-Fi | On-demand 2.4 GHz STA scan demo | `main/demo_wifi.c` | Scans only; it does not connect, store credentials, or validate antenna/RF performance |
| Bluetooth LE | On-demand non-connectable NimBLE advertising as `FoloPassport` | `main/demo_ble.c` | ESP32-C3 does not support Bluetooth Classic; radio range, coexistence, and power draw require device measurements |
| Low power | Two-second light sleep and five-second deep sleep, both with RTC timer wakeup | `main/demo_low_power.c` | Deep sleep restarts the application; the current demo exposes RTC timer wake only |
| Shared bus | ES8311 and CW2017 share I2C0 | `bsp_i2c_*` | Every device must reuse the bus owned by the BSP; do not create another bus on the same port for scanning or a new device |
| Logging and flashing | Native ESP32-C3 USB Serial/JTAG | ESP-IDF console | GPIO18/19 are reserved for USB; the default UART0 TX on GPIO21 conflicts with the backlight |

All pins, addresses, panel parameters, and button voltage windows are defined only in [`components/bsp/include/bsp_pins.h`](../components/bsp/include/bsp_pins.h). Application code must not duplicate these constants. See the [AI Hardware Development Guide](hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md) for the complete pin map, panel initialization, ADC thresholds, I2C addressing rules, audio clocks, and memory details.

Applications may also use ESP-IDF timers, FreeRTOS tasks, and internal Flash/NVS; the Pomodoro branch contains an NVS example. Wi-Fi and Bluetooth LE remain ESP-IDF application services rather than BSP APIs: their menu pages initialize each stack only while open and release it on exit. `demo/claude-buddy-port` remains a fuller BLE application architecture reference, not a substitute for measuring the current board's antenna, RF performance, power consumption, and coexistence behavior. The current product and firmware baseline uses 8 MB Flash with a 3 MB factory-app partition plus fixed protected identity and permanent-Recovery regions so derivative firmware stays installable through the mini-program.

### Capabilities outside the current contract

The public firmware contract is limited to the interfaces listed above. Do not infer additional board interfaces from the ESP32-C3 feature list. New hardware interfaces require an explicit BSP definition and on-device acceptance criteria.

## Start development with one requirement

A simple request can be given directly to an AI assistant:

```text
Build an offline habit-tracking application for FoloToy AI Passport.
Use the three physical buttons and the 240×320 display, and preserve records across power loss.
Start from `main`, create a `feature/*` branch, and develop the application there.
Follow AGENTS.md and docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md. Inspect relevant demo branches and plays/ applications first,
keep hardware logic in components/bsp and application logic in main, deliver a runnable
implementation with tests, and report the build result, unexecuted device checks, and exact
on-device acceptance steps separately.
```

Before starting, check [`plays/`](../plays/README.md) for an existing or reference
application and the demo branches, and [`docs/development/experience-notes.md`](development/experience-notes.md)
for previously recorded, reusable experience. See what is already built and reusable.

The more specific the requirement, the more likely the assistant is to implement it correctly in one pass. Useful details include:

- User flow: what each page displays and what short press, double press, and long press do for each button.
- State and data: whether the application needs timing, persistence across power loss, networking, recording, or communication with a computer.
- Experience goals: fonts, colors, animation, sound, response time, and error states.
- Constraints: whether the main menu may be replaced, dependencies added, Flash used, or default interactions changed.
- Acceptance criteria: which behaviors require automated tests and which must be observed on real hardware.

When details are omitted, the assistant may choose conservative defaults that do not change the product direction, but it must list those assumptions in the delivery. Decisions involving new wiring, electrical safety, board revisions, or irreversible data formats require confirmation first.

## Demo branches are design cases, not a feature pile

Each `demo/*` branch evolves the baseline into an independent application. The branches demonstrate how specific problems were solved. New applications should normally branch from `main` and consult relevant examples instead of merging multiple demos wholesale.

| Branch | Application | Patterns worth reusing |
| --- | --- | --- |
| `demo/stopwatch` | Stopwatch | Minimal timer application, separation of pure logic from LVGL, host-side logic tests |
| `demo/cat-themed-pomodoro-timer` | Cat-themed Pomodoro timer | Monotonic time, pause/resume, NVS persistence, a detailed PRD, and a state model |
| `demo/rock-paper-scissors` | Rock paper scissors | RGB565 image assets, asset-generation scripts, and Flash resource tradeoffs |
| `demo/tetris-game` | Three-button Tetris | Real-time game loop, low-latency `PRESS` input, partial refresh, a pure game model, audio, and microphone interaction |
| `demo/claude-buddy-port` | Desktop AI hardware companion | Replacing the demo menu with a complete application, encrypted BLE, protocol parsing, state reduction, task communication, and extensive host tests |

Inspect an example without switching the current working tree:

```bash
git branch -r --list 'origin/demo/*'
git diff main...origin/demo/tetris-game -- main components tests
git show origin/demo/tetris-game:main/demo_tetris.c
```

Start a new application. This repository hosts several independent projects on one baseline: after starting from `main`, create a `feature/*` branch and develop the application there — do not develop directly on `main`. Each project's final branch is `feature/*` (e.g. `feature/my-passport-app`), kept separate so `main` stays a clean upstream baseline and the projects do not entangle.

```bash
git switch main
git switch -c feature/my-passport-app
```

Example branches may change the same menu, configuration, or driver in incompatible ways. Understand the differences before extracting a state model, asset pipeline, or concurrency pattern. Code appearing in an example branch is not automatically part of the current `main` BSP contract.

## Project structure

```text
components/bsp/include/  Public BSP APIs and bsp_pins.h hardware facts
components/bsp/src/      Display, button, audio, battery, and shared-I2C implementations
main/                    Minimal menu, LVGL UI, and independent hardware demo pages
tests/                   Lightweight logic tests that can run without hardware
tools/                   Shared local/CI validation and firmware verification scripts
docs/                    Project docs, changelog, engineering/contribution rules, and design references
.github/                 GitHub community files, PR template, issue forms, and CI workflows
sdkconfig.defaults       ESP32-C3, USB console, Flash, and LVGL defaults
partitions.csv           App plus protected identity/Recovery layout
dependencies.lock        Reproducible ESP-IDF Managed Component resolution
AGENTS.md                Mandatory AI-agent entry point (paired with AGENTS.zh_CN.md)
CLAUDE.md                Claude Code pointer to AGENTS.md (paired Chinese version)
LICENSE                  Repository license
```

## Documentation

- [`docs/INDEX.md`](INDEX.md) — complete documentation index for contribution, engineering, fork, software, and hardware topics.
- [`docs/development/agent-guide.md`](development/agent-guide.md) — AI-assisted development workflow, source priorities, BSP boundaries, runtime rules, and delivery format.
- [`docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md`](hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md) — pin map, acceptance matrix, and troubleshooting guide.
- [`AGENTS.md`](../AGENTS.md) — mandatory entry point for AI-assisted work.
- [`docs/fork-guide.md`](fork-guide.md) — fork branch and documentation workflow.

> This README describes the product and repository. AI agents must begin with `AGENTS.md` and follow its task-specific routing.
