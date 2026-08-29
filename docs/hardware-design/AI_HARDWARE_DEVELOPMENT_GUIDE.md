<p align="right">
  <a href="AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# FoloToy AI Passport Hardware Development Guide

This is the board-level context for AI coding assistants and new developers. It records confirmed hardware facts, software architecture, invariants, extension points, and acceptance methods; it does not replace component datasheets.

> For firmware behavior, use `components/bsp/include/bsp_pins.h` and the BSP implementation as the source of truth. Do not copy assumptions from a generic ESP32-C3 board.

Document scope:

- Applicable target: the ESP32-C3 FoloToy AI Passport mapping implemented by this repository.
- Product specifications are in [specifications.md](specifications.md); firmware behavior follows `bsp_pins.h`, BSP implementations, `sdkconfig.defaults`, `partitions.csv`, and the demo code.
- Code audit date: 2026-08-26.

## 1. Before changing hardware-facing code

1. Read `AGENTS.md`, this guide, and the affected BSP header/implementation.
2. Run `git status --short --branch` and preserve unrelated changes.
3. Put reusable hardware behavior in `components/bsp`; keep menu, animation, product interaction, and validation pages in `main`.
4. Keep pins, I2C addresses, and panel dimensions in `bsp_pins.h` only.
5. Keep hardware-facing changes within the product specification and explicit BSP definitions.

## 2. Board overview

The target is the ESP32-C3 FoloToy AI Passport with ESP-IDF 5.5.3. It has 8 MB Flash and no PSRAM; display, audio, radio, tasks, and DMA compete for internal RAM.

| Subsystem | Device or mode | Resource | Firmware support |
| --- | --- | --- | --- |
| MCU | ESP32-C3 | 8 MB Flash, no PSRAM | Configured |
| Display | ST7789P3, 240 × 320, RGB565 | SPI2, 40 MHz, mode 0 | Driver and validation page |
| Backlight | LCD LED | GPIO21, LEDC 5 kHz/10 bit | PWM brightness control |
| Buttons | UP/DOWN/OK resistor ladder | GPIO0 / ADC1_CH0 | Events and live-voltage page |
| Audio | ES8311 playback and microphone | shared I2C + I2S0 full duplex | Playback and recording page |
| Battery | CW2017 fuel gauge | shared I2C0, address `0x63` | Optional SOC and voltage driver |
| Wi-Fi | 2.4 GHz station | initialized by the demo | Scan page |
| Bluetooth LE | NimBLE peripheral | initialized by the demo | Non-connectable advertising page |
| Low power | light/deep sleep | RTC timer wake | 2 s light and 5 s deep-sleep modes |
| Console | USB Serial/JTAG | native USB GPIO18/19 | Configured |

## 3. Pin map and resource ownership

This table describes the signals allocated by the current BSP and build configuration.

| GPIO | Function | Direction/peripheral | Notes |
| ---: | --- | --- | --- |
| 0 | three-button ADC node | ADC1_CH0 input | external 10 kΩ pull-up; boot-related pin |
| 1 | LCD CS | SPI output | ST7789P3 chip select |
| 2 | I2S DOUT | output | MCU to ES8311 |
| 3 | I2S WS | output | MCU is I2S master |
| 4 | I2S DIN | input | ES8311 to MCU |
| 5 | I2S BCLK | output | shared by TX/RX |
| 6 | I2S MCLK | output | required by codec configuration |
| 7 | I2C SCL | bidirectional open drain | ES8311 and CW2017 share I2C0 |
| 8 | LCD SCLK | SPI output | SPI2, 40 MHz, mode 0 |
| 9 | LCD MOSI | SPI output | no MISO; display cannot be read |
| 10 | I2C SDA | bidirectional open drain | internal pull-up enabled; suitable external pull-ups still expected |
| 18/19 | USB Serial/JTAG | USB | reserve for console |
| 20 | LCD DC | output | command/data select |
| 21 | backlight PWM | LEDC output | conflicts with common UART0 default TX |

LCD reset and amplifier enable are `-1`: display reset uses software reset, and the amplifier is treated as always enabled. A GPIO absent from this table is not automatically free.

### 3.1 Peripheral ownership and coexistence

| Resource | Owner | Sharing rule or conflict |
| --- | --- | --- |
| SPI2 | display BSP | Dedicated to the ST7789P3 in current firmware; no MISO is configured. |
| LEDC low-speed timer 0/channel 0 | backlight BSP | New PWM users must select a non-conflicting timer/channel and recheck clock changes. |
| ADC1 / channel 0 | button BSP | One oneshot unit is shared by button decoding and live-voltage reads; do not create a second ADC1 owner. |
| I2C0 | `bsp_i2c` | ES8311 and CW2017 share the single bus handle; clients must not recreate the bus. |
| I2S0 | audio BSP | TX and RX are full duplex and share MCLK/BCLK/WS. |
| USB Serial/JTAG | console configuration | GPIO18/19 are part of the selected console path. |
| Internal RAM/DMA | display, LVGL, audio, radio, tasks | No PSRAM exists; total free heap and largest contiguous block both matter. |
| NVS/network event loop | `demo_radio.c` | Prepared once for Wi-Fi/BLE demos; do not erase unrelated NVS data on initialization errors. |
| Wi-Fi/BLE stacks | individual demo pages | Current demos start on page entry and deinitialize on exit; the stacks do not remain active together. |

GPIO0 is both the button ADC node and an ESP32-C3 boot-related pin. GPIO21 is the backlight output and conflicts with the commonly used UART0 TX mapping. Pin reassignment requires boot/programming-path review and on-device acceptance.

### 3.2 Product interfaces outside the BSP

- USB Type-C 2.0 accepts 5 V input; firmware flashing and logs use the ESP32-C3 USB Serial/JTAG interface.
- The dedicated power button controls hardware power and is separate from the three ADC function buttons exposed by the BSP.
- The NTAG213 is a passive NFC tag and has no MCU-facing BSP API.
- LCD reset uses the controller's software-reset path, and amplifier enable is not controlled by an MCU GPIO.

## 4. Architecture and lifecycle

```text
app_main
  ├─ shared I2C init and scan
  ├─ display and LVGL init, then backlight
  ├─ button init
  ├─ audio init
  ├─ battery init
  └─ LVGL menu and independent demo pages
```

Display/LVGL is a hard dependency. Buttons, audio, and battery are soft dependencies whose pages show `[FAIL]` while other pages remain available. Public BSP APIs are under `components/bsp/include/`; most initialization is idempotent, but there is no universal BSP deinitialization API.

Wi-Fi, NimBLE, and sleep use ESP-IDF directly rather than the BSP. `demo_radio.c` owns shared NVS, `esp_netif`, and default-event-loop setup. Wi-Fi and Bluetooth pages allocate their radio stacks on entry and stop/deinitialize them on exit. Do not erase NVS to hide partition errors. Deep sleep restarts the application and the demo uses RTC slow memory for the wake counter.

## 5. Display and LVGL

- Panel: ST7789P3, 240 × 320 portrait, RGB565, SPI2 MOSI-only at 40 MHz, mode 0.
- `BSP_LCD_INVERT_COLOR=1`; change inversion only after measurement with the replacement panel.
- Reset is software-only, gap is `(0, 0)`, X/Y mirroring is disabled, and LVGL rotation may override lower-level mirror settings.
- The vendor porch, power, and gamma sequence in `bsp_display.c` is panel-specific. Do not treat it as a universal ST7789 sequence.
- `swap_bytes=true` is required because LVGL emits little-endian RGB565 while SPI sends the high byte first.

The LVGL DMA buffer is one `240 × 20` RGB565 buffer, about 9.6 KB; the LVGL internal pool is 24 KB. Do not add large/double buffers without checking internal RAM, the largest contiguous heap block, and I2S DMA.

LVGL is not thread-safe. Timer callbacks in LVGL context may access objects directly. Button callbacks and worker tasks must use `bsp_lvgl_lock()`/`bsp_lvgl_unlock()`. Stop producers before deleting a page and clear static object pointers afterward.

## 6. ADC button ladder

GPIO0 has an external 10 kΩ pull-up to 3.3 V. UP, DOWN, and OK connect it to ground through 0 Ω, 1 kΩ, and 2.2 kΩ respectively.

| State | Nominal voltage | Current window |
| --- | ---: | ---: |
| UP | about 0 mV | `[0, 150)` mV |
| DOWN | about 300 mV | `[150, 447)` mV |
| OK | about 595 mV | `[447, 1900)` mV |
| Released | about 3300 mV | outside all windows |

Do not replace the external resistor with the inaccurate internal pull-up. The BSP creates one ADC1 oneshot unit and shares it with all button devices and voltage reads. Attenuation is `ADC_ATTEN_DB_12`. Callbacks originate in the button component task and must not block or perform heavy UI work.

Calibrate thresholds using multiple boards, charge levels, and reasonable temperatures; leave margin between measured distributions rather than relying only on divider theory.

## 7. Shared I2C

I2C0 uses SDA GPIO10 and SCL GPIO7. ES8311 is 7-bit address `0x18`; CW2017 is `0x63`. `bsp_i2c.c` exclusively owns the bus.

- Never create a second temporary bus on the same port for probing or a device.
- Scan with `i2c_master_probe()` on the existing bus. The scan covers `0x08` through `0x77`; success means the scan completed, not that a device was found.
- CW2017 runs at 100 kHz. ES8311 control is managed by `esp_codec_dev`.
- The codec control API expects an 8-bit address, so ES8311 receives `0x18 << 1`; do not copy that shift into 7-bit ESP-IDF APIs.

Troubleshoot in order: bus-init log, scan results for `0x18`/`0x63`, power/ground/wiring/pull-ups, address format, and accidental duplicate-bus creation.

## 8. ES8311 audio

The MCU is I2S master and the ES8311 is slave. I2S0 TX/RX shares MCLK GPIO6, BCLK GPIO5, and WS GPIO3; DOUT is GPIO2 and DIN is GPIO4. The demo opens 16 kHz, 16-bit, mono PCM over a physically two-slot standard-I2S bus.

- Call `bsp_audio_set_format()` before PCM I/O.
- A format change must close and reopen `esp_codec_dev`; an already open device is not reconfigured.
- Preserve the I2S enable/disable sequence around close/open.
- Do not write ES8311 clock-divider registers after open; the driver derives them from sample rate and 256×fs MCLK.
- Keep `no_dac_ref=true` for mono microphone input; false can produce all-zero capture.
- Microphone analog gain is 30 dB; output volume is a separate 0–100% value.
- `bsp_audio_read/write` block and must not run in button callbacks or the LVGL task.
- I2S DMA uses six descriptors of 240 frames each.

The audio demo's three-second recording buffer is about 96 KB and is the largest transient heap allocation. Prefer chunked streaming for longer audio. Production task shutdown needs a cancellable loop and explicit exit handshake rather than deleting a task blocked in codec I/O.

## 9. CW2017 fuel gauge

Initialization reads VERSION, writes CONFIG `0x00`, waits 100 ms, and uses the chip's built-in Li-Poly profile. The repository intentionally does not write a custom cell profile.

- SOC uses registers `0x04–0x05`; values above 100 are treated as not ready and return `-1`.
- Voltage uses the 14-bit value at `0x02–0x03`, converted as `raw × 312.5 µV`, and returned in mV.
- Transactions use a 100 ms timeout at 100 kHz.
- A missing device returns `ESP_ERR_NOT_FOUND`; the battery page is disabled without stopping the application.

Accurate production SOC requires the cell parameters, CW2017 datasheet/vendor profile, and full charge/discharge validation.

## 10. Flash, console, and memory

The current product and firmware baseline uses 8 MB Flash. `sdkconfig.defaults` fixes the image to 8 MB and disables automatic flash-size header rewriting. `partitions.csv` defines 24 KB NVS, 4 KB PHY data, one 3 MB factory application, protected `cardid` at `0x356000`, and permanent Recovery at `0x700000`. This is not an ESP-IDF dual-slot OTA layout: the factory-installed Recovery performs BLE installation and must remain at its fixed address. The bootloader enters it when UP/GPIO0 is held for five seconds. A detected non-8-MB device does not match this baseline; identify the board and flash part before changing the project default.

Do not erase a provisioned device or move/overlap the protected partitions.
Community firmware contains neither device identity nor a replacement Recovery
payload. See the [BLE compatibility contract](../development/ble-recovery-compatibility.md).

The console is USB Serial/JTAG. Do not switch to the UART0 default output without resolving its GPIO21 conflict with the backlight.

Review at least the 24 KB LVGL pool, 9.6 KB LCD DMA buffer, I2S DMA, 96 KB demo recording, radio stacks, task stacks, total free heap, and largest contiguous block when adding assets, TLS/networking, audio buffers, or double buffering.

## 11. Adding features

For reusable hardware capability, add `bsp_<feature>.h` and its implementation, keep constants in `bsp_pins.h`, update component CMake/dependencies, return `esp_err_t`, log actionable pin/address context, and document threading, blocking, ownership, initialization, and failure behavior.

For a validation page, implement `enter`, `exit`, and `key` in `main/demo_<feature>.c`; declare it in `demo.h`, list it in CMake, and register it in `DEMOS[]`. Create/load a page-owned screen on entry. Stop workers/timers before deleting it on exit. Keep UI text in English, put slow work in worker tasks, lock LVGL updates, and preserve global OK-long-press return behavior.

Menu initialization status arrays implicitly follow `DEMOS[]` order; update and review them together.

## 12. Development environment

Follow the canonical [environment bootstrap](../development/environment-setup.md)
for clean-machine installation, OS-specific prerequisites, and international or
mainland China download routes. Use ESP-IDF 5.5.3 outside the repository,
activate its `export.sh` in every terminal, and confirm the exact version.
Prefer `./tools/validate.sh --firmware` to build the verified merged image and
flash that image at `0x0`; use direct `idf.py build/flash` only for incremental
development.

```bash
source <path-to-esp-idf-v5.5.3>/export.sh
idf.py --version
idf.py set-target esp32c3
idf.py reconfigure
idf.py build
```

The Component Manager resolves dependencies from `components/bsp/idf_component.yml`. Do not edit `managed_components/`. `dependencies.lock` is tracked and must remain reproducible under ESP-IDF 5.5.3. Generated `sdkconfig` does not automatically absorb every changed default; preserve intentional settings and use `idf.py set-target esp32c3` when configuration must be regenerated. Use `idf.py fullclean` only to remove stale build output.

For an intentional incremental flash, use the native USB Serial/JTAG port,
commonly `/dev/ttyACM0` on Linux:

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

The actual port may differ. Check the cable, enumeration, permissions, power, and download mode before changing USB GPIOs or console configuration. Avoid running `idf.py` permanently as root.

## 13. Build and device validation

Run `./tools/validate.sh` for the complete automated gate. A successful build is the minimum automated result, not physical-device acceptance.

General board acceptance:

- Stable USB Serial/JTAG logs without reboot loops, assertions, watchdogs, or persistent errors.
- I2C scan sees ES8311 at `0x18` and, when fitted, CW2017 at `0x63`.
- UP/DOWN wraps menu navigation, OK click enters, and OK long press returns.
- An optional peripheral failure disables only its page.
- Repeated navigation and operation do not leak heap, tasks, timers, or objects.

| Change | Required physical observations |
| --- | --- |
| Pin/I2C | scan, all shared devices, boot straps, USB logs |
| LCD | color blocks, orientation, clipping, inversion, byte order, backlight levels |
| ADC/buttons | released and pressed mV, click/double/long events, margin across battery levels |
| Codec/I2S | 1 kHz tone, non-zero recording, correct playback speed, format changes, page exit |
| Battery | plausible SOC/mV, graceful missing-device behavior, intermittent-I2C recovery |
| Wi-Fi | visible scan count/SSID/RSSI, rescan, repeated entry/exit |
| Bluetooth LE | phone sees `FoloPassport`, restart advertising, advertising stops on exit, repeated entry/exit |
| Light/deep sleep | select with UP/DOWN; 2 s light sleep resumes with backlight; 5 s deep sleep restarts with timer cause and retained count |
| DMA/memory/UI | build memory report, runtime minimum heap/largest block, stable concurrent audio/display |

## 14. Troubleshooting

| Symptom | Check first |
| --- | --- |
| Backlight but no image | CS/DC/MOSI/SCLK, vendor sequence, software reset, display-on, SPI mode |
| Wrong colors | byte swap, RGB/BGR, inversion; change one variable at a time |
| Rotation change has no effect | LVGL rotation overriding lower-level mirror |
| Backlight or console failure | GPIO21 conflict with UART0 default TX |
| Button confusion | external 10 kΩ pull-up, measured voltage, thresholds, attenuation |
| `adc1 is already in use` | accidental second ADC1 oneshot unit |
| Both I2C devices disappear | accidental second I2C0 bus |
| Only ES8311 missing | address API shift and codec power |
| Audio speed/pitch wrong | close/open on format change, sample rate/MCLK, no manual clock writes |
| Recording is zero | `no_dac_ref`, DIN GPIO4, microphone path, gain |
| Recording allocation fails | no PSRAM; shorten/stream and inspect largest block |
| Battery shows `--` | `0x63` response, invalid SOC, profile/startup delay |
| Wi-Fi/BLE fails on second entry | stack stop/deinit and one-time NVS/event-loop setup |
| Black after light sleep | timer wake source, sleep error, backlight restore |
| Deep sleep does not restart | timer source, boot wake cause, RTC counter |
| I2S allocation fails after UI growth | competition among LCD/LVGL buffers and I2S DMA |
| Chinese text appears as boxes | Montserrat 14/20 has no CJK glyphs; compile and select a CJK subset, configure fallback for mixed text, and verify glyph coverage on the device |

## 15. Pre-delivery checklist

- [ ] No duplicated pins, addresses, dimensions, or board parameters.
- [ ] No unsupported capability presented as confirmed fact.
- [ ] No second I2C0 bus or ADC1 unit.
- [ ] Non-LVGL contexts lock LVGL access.
- [ ] Blocking hardware work stays out of button callbacks and the LVGL task.
- [ ] Page exit prevents background access to deleted objects.
- [ ] Audio format changes retain close/open and required codec settings.
- [ ] Memory review includes LVGL, LCD DMA, I2S DMA, task stacks, and largest block.
- [ ] Automated validation passed or the actual failure is reported.
- [ ] Build results and observed device results are reported separately.
- [ ] The diff contains only task-scoped changes and preserves user work.
