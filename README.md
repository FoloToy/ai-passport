# FoloToy AI Passport

English | [简体中文](README.zh_CN.md)

FoloToy AI Passport is open wearable AI hardware designed for AI agents. This repository is the development baseline for the device. It goes beyond showing “what the board can run” by keeping the **hardware facts, stable interfaces, resource boundaries, reference implementations, and validation methods** that an agent needs to build applications in one place.

The repository is organized around the following principles:

- `main` is the smallest complete runnable baseline and an executable description of the current hardware capabilities.
- `components/bsp` isolates board-level details and exposes stable APIs to applications.
- `demo/*` branches show different paths from a product requirement to a working implementation.
- `AGENTS.md` defines how an agent should work in the repository, while `docs/AI_HARDWARE_DEVELOPMENT_GUIDE.md` contains the complete hardware context and troubleshooting knowledge.
- Build results and physical-device results are reported separately. A successful build must never be presented as successful hardware validation.

The intended workflow is simple: give an agent this repository and an application requirement. The agent identifies the available capabilities and constraints, selects relevant examples, implements and builds the application, and returns an acceptance checklist that can be executed on the physical device.

## Entry point for AI agents

Before starting development, establish context in this order:

1. Read `AGENTS.md`, this README, and [`docs/AI_HARDWARE_DEVELOPMENT_GUIDE.md`](docs/AI_HARDWARE_DEVELOPMENT_GUIDE.md).
2. Run `git status --short --branch` and preserve all existing user changes.
3. Read the affected `components/bsp/include/*.h` headers and their implementations. Do not infer board behavior from common chip or development-board configurations.
4. Use `git branch -r --list 'origin/demo/*'` to find examples close to the requirement. Reuse only the relevant design patterns; do not merge an entire demo branch by default.
5. Break the requirement into inputs, outputs, state, concurrent tasks, persistence, memory budget, and failure degradation before deciding whether to change `main` or extend `components/bsp`.
6. Complete the minimum build check and all applicable logic tests. Keep explicit on-device acceptance items for every conclusion that depends on the display, buttons, audio, battery, or timing.

### Source-of-truth priority

When information conflicts, use this priority order:

```text
Schematic / PCB / board revision / physical measurement
    > components/bsp/include/bsp_pins.h
    > BSP public headers and implementations
    > docs/AI_HARDWARE_DEVELOPMENT_GUIDE.md
    > README and example applications
```

The repository does not currently include schematic or PCB source files. When the board revision, wiring, polarity, register behavior, or unused GPIOs are unknown, an agent must report the unknown and request evidence instead of filling the gap with parameters from another ESP32-C3 board.

## Hardware capability contract

The table below describes the application capabilities implemented by the current `main` branch. It is not a list of everything that might be possible according to the chip datasheet.

| Capability | Confirmed implementation | Application interface | Boundaries that must be respected |
| --- | --- | --- | --- |
| Display | ST7789P3, 240 × 320 portrait RGB565, SPI2 at 40 MHz; LEDC backlight | `bsp_display_*`, `bsp_lvgl_*` | The ESP32-C3 has no PSRAM; the current design uses a small single DMA buffer; no LCD MISO, touch, or known TE interface |
| Input | `UP`, `DOWN`, and `OK` share an ADC resistor ladder on GPIO0 | `bsp_button_init()`, `bsp_button_read_mv()` | Callbacks run in the button component task and must not block; do not create a second ADC1 unit |
| Audio | ES8311 with full-duplex PCM over I2S0, supporting playback and microphone capture | `bsp_audio_*` | PCM reads and writes block and belong in a worker task; format changes must retain the BSP close/open sequence |
| Battery | CW2017 state-of-charge and voltage readings | `bsp_battery_*` | This capability is optional at runtime; accuracy depends on the cell and battery profile and is not equivalent to a calibrated result |
| Wi-Fi | On-demand 2.4 GHz STA scan demo | `main/demo_wifi.c` | Scans only; it does not connect, store credentials, or validate antenna/RF performance |
| Bluetooth LE | On-demand non-connectable NimBLE advertising as `FoloPassport` | `main/demo_ble.c` | ESP32-C3 does not support Bluetooth Classic; radio range, coexistence, and power draw require device measurements |
| Low power | Two-second light sleep and five-second deep sleep, both with RTC timer wakeup | `main/demo_low_power.c` | Deep sleep restarts the application; external/button wakeup and board-level power consumption remain unverified |
| Shared bus | ES8311 and CW2017 share I2C0 | `bsp_i2c_*` | Every device must reuse the bus owned by the BSP; do not create another bus on the same port for scanning or a new device |
| Logging and flashing | Native ESP32-C3 USB Serial/JTAG | ESP-IDF console | GPIO18/19 are reserved for USB; the default UART0 TX on GPIO21 conflicts with the backlight |

All pins, addresses, panel parameters, and button voltage windows are defined only in [`components/bsp/include/bsp_pins.h`](components/bsp/include/bsp_pins.h). Application code must not duplicate these constants. See the [AI Hardware Development Guide](docs/AI_HARDWARE_DEVELOPMENT_GUIDE.md) for the complete pin map, panel initialization, ADC thresholds, I2C addressing rules, audio clocks, and memory details.

Applications may also use ESP-IDF timers, FreeRTOS tasks, and internal Flash/NVS; the Pomodoro branch contains an NVS example. Wi-Fi and Bluetooth LE remain ESP-IDF application services rather than BSP APIs: their menu pages initialize each stack only while open and release it on exit. `demo/claude-buddy-port` remains a fuller BLE application architecture reference, not a substitute for measuring the current board's antenna, RF performance, power consumption, and coexistence behavior. Every FoloToy AI Passport has 8 MB of Flash, and the default firmware configuration targets 8 MB with a 3 MB factory-app partition.

### Capabilities outside the current contract

The repository does not currently provide enough evidence to guarantee touch input, display readback, an IMU, external storage, charging control, USB insertion detection, controllable power-amplifier enable, external/button deep-sleep wakeup, arbitrary “free GPIOs,” exact battery capacity, or production-grade power specifications. A capability being present in the ESP32-C3 silicon does not mean that it is connected, powered correctly, or validated on this board.

Requirements involving these capabilities must begin with a schematic, board revision, component documentation, or physical measurements. Only then should the BSP and its acceptance criteria be extended.

## Start development with one requirement

A simple request can be given directly to an agent:

```text
On the main branch, build an offline habit-tracking application for FoloToy AI Passport.
Use the three physical buttons and the 240×320 display, and preserve records across power loss.
Follow AGENTS.md and AI_HARDWARE_DEVELOPMENT_GUIDE.md. Inspect relevant demo branches first,
keep hardware logic in components/bsp and application logic in main, deliver a runnable
implementation with tests, and report the build result, unexecuted device checks, and exact
on-device acceptance steps separately.
```

The more specific the requirement, the more likely the agent is to implement it correctly in one pass. Useful details include:

- User flow: what each page displays and what short press, double press, and long press do for each button.
- State and data: whether the application needs timing, persistence across power loss, networking, recording, or communication with a computer.
- Experience goals: fonts, colors, animation, sound, response time, and error states.
- Constraints: whether the main menu may be replaced, dependencies added, Flash used, or default interactions changed.
- Acceptance criteria: which behaviors require automated tests and which must be observed on real hardware.

When details are omitted, an agent may choose conservative defaults that do not change the product direction, but it must list those assumptions in the delivery. Decisions involving new wiring, electrical safety, board revisions, or irreversible data formats require confirmation first.

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

Start a new application:

```bash
git switch main
git switch -c feature/my-passport-app
```

Example branches may change the same menu, configuration, or driver in incompatible ways. An agent must understand the differences before extracting a state model, asset pipeline, or concurrency pattern. Code appearing in an example branch is not automatically part of the current `main` BSP contract.

## Application and BSP boundary

```text
Natural-language requirement
  └─ main/                         Pages, state machines, animation, app tasks, assets
      └─ components/bsp/include/  Stable board-level APIs
          └─ components/bsp/src/  GPIO, buses, devices, and driver details
              └─ bsp_pins.h       Single source of truth for pins and hardware parameters
```

To add a regular page, create `main/demo_<feature>.c` and implement the `enter`, `exit`, and `key` interface, then update:

- Declarations in `main/demo.h`.
- The source list in `main/CMakeLists.txt`.
- The `DEMOS[]` registration in `main/main.c`.
- Menu initialization status and failure degradation if a new optional peripheral is involved.

Only hardware capabilities shared by multiple applications belong in `components/bsp`. A BSP API must document blocking behavior, thread context, memory ownership, failure values, and initialization order. Pins and I2C addresses belong only in `bsp_pins.h`.

### Runtime invariants

- LVGL is not thread-safe. Code outside the LVGL context must hold `bsp_lvgl_lock()` while accessing `lv_*` objects.
- Button callbacks only dispatch lightweight events. Recording, playback, storage, and other slow operations belong in worker tasks.
- When leaving a page, stop every task or timer that may access its UI before deleting the screen and clearing object pointers.
- The default global interaction is `UP`/`DOWN` navigation in the menu, short `OK` to enter, and long `OK` to return from a page. Any change must be explicit.
- New images, fonts, network stacks, audio buffers, LVGL buffers, and task stacks must be evaluated against internal RAM. Sufficient total free heap does not guarantee a sufficiently large contiguous block.
- Testable state machines, protocols, timing, and layout calculations should be separated from ESP-IDF/LVGL and covered by host-side logic tests.

## Build and run baseline

The project uses ESP-IDF 5.5.x; the known development environment is 5.5.3:

```bash
get_idf553                    # Maintainer-local helper
# Or source "$HOME/esp/esp-idf-v5.5.3/export.sh" (example installation path)
idf.py set-target esp32c3     # Run for a fresh checkout or after using another target
idf.py build
idf.py flash monitor
```

The first build uses ESP-IDF Component Manager to fetch LVGL, `esp_lvgl_port`, `button`, `esp_codec_dev`, and other dependencies. Do not edit the generated `managed_components/` directory. If configuration state is stale, use `idf.py fullclean` and configure again, but never use it to clean user source changes.

The current baseline includes a pure-logic test that can run independently:

```bash
cc -std=c11 -Wall -Wextra -Werror -Imain \
  tests/test_ui_pixel_math.c main/ui_pixel_math.c \
  -o /tmp/test_ui_pixel_math
/tmp/test_ui_pixel_math
```

Different example branches may provide their own host-test commands; follow the README on that branch.

## Acceptance and delivery format

`idf.py build` is the minimum automated check, not hardware validation. For changes involving physical peripherals, record at least the following on a FoloToy AI Passport:

- USB Serial/JTAG produces stable startup logs with no reboot loop, assertion, or watchdog reset.
- Display orientation, colors, edges, refresh behavior, and backlight are correct.
- `UP`, `DOWN`, and `OK` produce the intended events, and long `OK` returns correctly.
- Audio sample rate, playback, non-zero recording, and page exit behavior are correct.
- Battery readings are plausible, and the application degrades safely when the CW2017 is absent.
- Wi-Fi rescans complete, BLE advertising is visible to a phone scanner, and both pages can be entered and exited repeatedly.
- The Low Power page uses `UP`/`DOWN` to select light or deep sleep and `OK` to run it. Light sleep wakes by timer after approximately two seconds and restores the display backlight; deep sleep restarts after approximately five seconds and reports a retained wake count.
- Repeated page transitions and concurrent operations do not continuously leak tasks, objects, or heap.

An agent's final delivery must distinguish these outcomes:

```text
Build: PASS / FAIL / NOT RUN
Host tests: PASS / FAIL / NOT RUN
Device tests: PASS / FAIL / NOT RUN
Unverified: items that still require a board, instrument, or user confirmation
```

See the [AI Hardware Development Guide](docs/AI_HARDWARE_DEVELOPMENT_GUIDE.md) for the acceptance matrix by change type—including pins, LCD, ADC, codec, I2C, and DMA—and the troubleshooting reference.

## Project structure

```text
components/bsp/include/  Public BSP APIs and bsp_pins.h hardware facts
components/bsp/src/      Display, button, audio, battery, and shared-I2C implementations
main/                    Minimal menu, LVGL UI, and independent hardware demo pages
tests/                   Lightweight logic tests that can run without hardware
docs/                    Agent hardware development guide and extension documentation
sdkconfig.defaults       ESP32-C3, USB console, Flash, and LVGL defaults
partitions.csv           NVS, PHY data, and 3 MB factory application layout
AGENTS.md                Coding, validation, and contribution rules for agents
```
