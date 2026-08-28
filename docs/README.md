<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# FoloToy AI Passport

This repository is the self-contained firmware-development baseline for every standard-production FoloToy AI Passport. An AI coding agent must be able to use the current checkout to establish ESP-IDF 5.5.3, implement an application, run host checks, build ESP32-C3 firmware, and produce the verified merged image without fetching remote demo branches.

## Hardware capability contract

`main` contains one runnable page for each supported baseline capability. `components/bsp` owns reusable board behavior and all firmware-visible hardware constants.

| Capability | Baseline implementation | Required boundary |
| --- | --- | --- |
| Display | ST7789P3, 240 × 320 RGB565, SPI2 at 40 MHz, LEDC backlight | Use `bsp_display_*` and `bsp_lvgl_*`; no PSRAM, LCD MISO, touch, or TE interface exists |
| Buttons | UP/DOWN/OK on the production-measured GPIO0 ADC ladder | Use `bsp_button_*`; callbacks do not block; one ADC1 owner only |
| Audio | ES8311 playback and microphone capture over full-duplex I2S0 | Run blocking PCM I/O in a worker task; retain the BSP format close/open sequence |
| Battery | Mandatory CW2017 SOC and voltage readings | Treat `ESP_ERR_NOT_FOUND` as a hardware fault; readings are not a calibrated cell-capacity claim |
| Wi-Fi | 2.4 GHz station scan | The demo does not connect or store credentials |
| Bluetooth LE | Non-connectable NimBLE advertising as `FoloPassport` | Bluetooth Classic is unsupported; device tests determine range and power draw |
| Low power | Light/deep sleep with GPIO0 button or RTC timer wake | Any function button wakes; timer fallback is 2 seconds for light sleep and 5 seconds for deep sleep |
| Shared I2C | ES8311 `0x18` and CW2017 `0x63` on I2C0 | Reuse `bsp_i2c`; never create a second bus on I2C0 |
| Flashing | Native USB Serial/JTAG and a merged 0x0 image | GPIO18/19 remain USB; GPIO21 remains the backlight |

The complete pin map, electrical facts, resource ownership, and physical acceptance matrix are in the [hardware guide](hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md). Application code must not duplicate constants from [`bsp_pins.h`](../components/bsp/include/bsp_pins.h).

## Give an AI agent one requirement

Use a request with observable behavior and acceptance criteria:

```text
Build an offline habit tracker for FoloToy AI Passport.
Use the three buttons and 240×320 display. Store records in NVS.
UP/DOWN selects a day; OK toggles completion; OK long press returns to the menu.
Follow AGENTS.md. Keep board logic in components/bsp and application logic in main.
Run the complete validation gate and output build/FoloToy-AI-Passport-full.bin.
Report Build, Host tests, Device tests, and Unverified separately.
```

The request must define the user flow, button behavior, persistent data, networking/audio use, resource constraints, and device acceptance. If one of those decisions changes observable behavior and is missing, the agent asks the user before implementation. The agent never invents wiring, pin assignments, electrical limits, board revisions, credentials, or irreversible storage formats.

## Project structure

```text
components/bsp/include/  Public BSP APIs and bsp_pins.h hardware facts
components/bsp/src/      Display, button, audio, battery, and shared-I2C implementations
main/                    Runnable menu, LVGL UI, and seven baseline demo pages
tests/                   Hardware-independent host tests
tools/                   Environment-independent checks and firmware verification
docs/                    Product, engineering, contribution, and hardware documents
.github/                 Community policy, templates, and three CI workflows
sdkconfig.defaults       ESP32-C3, 8 MB Flash, USB, sleep, and LVGL defaults
partitions.csv           NVS, PHY data, and one 3 MB factory application
dependencies.lock        Pinned ESP-IDF Managed Component resolution
AGENTS.md                Only mandatory AI-agent entry point
CLAUDE.md                Pointer to AGENTS.md for Claude Code
```

## Authoritative entry points

- [`AGENTS.md`](../AGENTS.md): mandatory task routing, safety rules, and delivery fields.
- [`development/agent-guide.md`](development/agent-guide.md): code-change workflow and runtime invariants.
- [`development/environment-setup.md`](development/environment-setup.md): clean-machine ESP-IDF 5.5.3 setup.
- [`development/build-and-test.md`](development/build-and-test.md): exact checks, firmware output, CI, and device-test boundary.
- [`hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md`](hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md): confirmed board facts and physical acceptance.

Use [`INDEX.md`](INDEX.md) only to discover additional maintained documents.
