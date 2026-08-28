<p align="right">
  <a href="AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# FoloToy AI Passport Hardware Development Guide

This guide defines the firmware-visible hardware for every shipped standard-production AI Passport. Product specifications and measured results have priority, followed by `bsp_pins.h`, BSP headers/implementation, this guide, and demo code. Ask the user before using a hardware detail absent from those sources.

Hardware and code facts were reconciled on 2026-08-28.

## Board and pin contract

The target is ESP32-C3, 8 MB Flash, no PSRAM, and ESP-IDF 5.5.3.

| GPIO | Production function | Electrical or ownership rule |
| ---: | --- | --- |
| 0 | UP/DOWN/OK ADC ladder; light/deep-sleep wake | External 10 kΩ pull-up to 3.3 V; active-low wake; ADC1_CH0 has one BSP owner |
| 1 | LCD CS | ST7789P3 on SPI2 |
| 2 | I2S DOUT | MCU to ES8311 |
| 3 | I2S WS | MCU is I2S master |
| 4 | I2S DIN | ES8311 to MCU |
| 5 | I2S BCLK | Shared by I2S0 TX/RX |
| 6 | I2S MCLK | 256× sample-rate clock |
| 7 | I2C SCL | I2C0; external 2.2 kΩ pull-up to 3.3 V |
| 8 | LCD SCLK | SPI2, 40 MHz, mode 0 |
| 9 | LCD MOSI | No LCD MISO exists |
| 10 | I2C SDA | I2C0; external 2.2 kΩ pull-up to 3.3 V |
| 18/19 | USB Serial/JTAG | Reserved for console and flashing |
| 20 | LCD DC | ST7789P3 command/data |
| 21 | LCD backlight | LEDC low-speed timer 0/channel 0, 5 kHz, 10 bit; conflicts with default UART0 TX |

LCD reset is not connected to the MCU and uses controller software reset. The speaker amplifier has no MCU enable pin, is powered directly from the battery, and uses 4.2 V as `esp_codec_dev`'s fixed full-charge gain-calibration value. A GPIO absent from the table is not a public application interface.

The dedicated power button belongs entirely to the external power circuit. Firmware cannot read it. A continuous 0.5-second hold starts the device; a continuous 2-second hold shuts it down. The passive NTAG213 has no MCU connection or BSP API.

## Resource ownership and lifecycle

```text
app_main
  -> initialize and scan BSP-owned I2C0
  -> initialize ST7789P3, LVGL, and backlight
  -> initialize ADC buttons, ES8311, and CW2017
  -> load the seven-page LVGL menu
```

| Resource | Owner | Rule |
| --- | --- | --- |
| SPI2 | display BSP | Dedicated to ST7789P3; no MISO |
| ADC1_CH0 | button BSP | Button decoding, live voltage, and wake preparation share one unit |
| I2C0 | `bsp_i2c` | ES8311 and CW2017 clients reuse one bus handle |
| I2S0 | audio BSP | TX/RX share MCLK, BCLK, and WS |
| Wi-Fi/BLE stacks | current demo page | Start on entry and deinitialize on exit; they do not coexist in the baseline |
| NVS/netif/event loop | `demo_radio.c` | Initialize once; never erase unrelated NVS after an initialization error |
| LVGL objects | LVGL task | Every other task or callback uses `bsp_lvgl_lock()` |

Display/LVGL is required for the menu. Button, audio, and CW2017 initialization failures mark their pages `[FAIL]`; all three devices remain mandatory production components. Before a page deletes its screen, it stops every task, timer, callback, and event source that can access that screen.

## Display and LVGL

The panel is ST7789P3, 240 × 320 portrait RGB565, SPI2 MOSI-only at 40 MHz, mode 0. Reset is software-only, gap is `(0, 0)`, X/Y mirroring is disabled, and inversion is enabled. The porch, power, and gamma commands in `bsp_display.c` are the production-panel sequence and must not be reused for another panel.

LVGL uses one `240 × 20` RGB565 DMA buffer: 9,600 bytes. The LVGL internal pool is 24 KiB. `swap_bytes=true` is required because LVGL provides little-endian RGB565 and the LCD SPI stream sends the high byte first. The baseline fonts are Montserrat 14 and 20 and contain no CJK glyphs.

Application pages use LVGL. Direct `esp_lcd_panel_draw_bitmap()` is allowed only when the PR includes a device measurement and verifies all four conditions: concurrent LVGL drawing is stopped, the display/LVGL lifecycle lock is held, every rectangle is in bounds, and RGB565 uses LCD byte order.

## ADC buttons and sleep wake

GPIO0 is pulled to 3.3 V through 10 kΩ. UP, DOWN, and OK connect the node to ground through 0 Ω, 1 kΩ, and 2.2 kΩ.

| State | Nominal voltage | Production-measured recognition window |
| --- | ---: | ---: |
| UP | 0 mV | `[0, 150)` mV |
| DOWN | 300 mV | `[150, 447)` mV |
| OK | 595 mV | `[447, 1900)` mV |
| Released | 3300 mV | outside every window |

The internal pull-up must not replace the external 10 kΩ resistor. Changing the resistor ladder, ADC attenuation, or PCB requires new released/UP/DOWN/OK measurements across the target battery and temperature range before changing `BSP_BTN_MV_TABLE`.

Every function button is an active-low wake source. `bsp_button_prepare_wakeup()` stops the button timer and changes GPIO0 from ADC to digital input without internal pulls. Light sleep uses GPIO low-level wake plus a 2-second timer; deep sleep uses GPIO low-level wake plus a 5-second timer. `CONFIG_ESP_SLEEP_GPIO_ENABLE_INTERNAL_RESISTORS=n` prevents a sleep pull-up from being placed in parallel with the production resistor. After light sleep, `bsp_button_resume_after_wakeup()` restores ADC1_CH0 and resumes button polling. Deep sleep restarts the application.

## Shared I2C

I2C0 uses SDA GPIO10 and SCL GPIO7. Each line has a 2.2 kΩ external pull-up to 3.3 V; firmware also enables internal pull-ups. ES8311 uses 7-bit address `0x18`; CW2017 uses `0x63` and 100 kHz. Both devices must appear in the production-board scan.

Only `bsp_i2c` creates the bus. Scanning uses `i2c_master_probe()` on the existing handle from `0x08` through `0x77`. A completed scan with zero devices is a diagnostic failure, not successful device discovery. The codec control adapter alone receives `0x18 << 1` because its API expects an 8-bit address; ESP-IDF I2C APIs continue to use 7-bit addresses.

## ES8311 audio

The MCU is I2S master. I2S0 TX/RX share MCLK GPIO6, BCLK GPIO5, and WS GPIO3; playback uses DOUT GPIO2 and capture uses DIN GPIO4. The demo opens 16 kHz, 16-bit, mono PCM on a physical two-slot standard-I2S bus.

- Call `bsp_audio_set_format()` before PCM I/O. A format change closes and reopens `esp_codec_dev`; removing this sequence leaves the old sample clock active.
- Keep `no_dac_ref=true`; false routes the mono input to the DAC reference and produces zero capture.
- Microphone analog gain is 30 dB. Output volume is a separate 0–100 value.
- The amplifier is battery-powered with no enable GPIO. The fixed `pa_voltage=4.2` value models full-charge gain compensation; it is not a regulated supply claim.
- PCM read/write blocks and runs in a worker task. I2S DMA uses six descriptors of 240 frames each.
- The three-second recording buffer is exactly 96,000 bytes at 16 kHz, 16-bit mono. A longer recording uses chunked processing or storage; it never assumes PSRAM.

## CW2017 battery gauge

Every production unit contains a 520 mAh cell and CW2017 at address `0x63`. Initialization reads VERSION, writes CONFIG `0x00`, waits 100 ms, and uses the chip's built-in Li-Poly profile. The repository does not write a custom cell profile.

SOC comes from `0x04–0x05`; a value above 100 returns `-1`. Voltage comes from the 14-bit value at `0x02–0x03` using `raw × 312.5 µV`. Transactions use a 100 ms timeout at 100 kHz. `ESP_ERR_NOT_FOUND` indicates a board, power, I2C, or soldering fault and disables only the battery page. The reported SOC is a gauge reading, not a full-cycle calibrated capacity result.

## Flash, console, and memory

The build uses 8 MB Flash, USB Serial/JTAG, and no PSRAM. The partition table is fixed:

| Partition | Offset | Size |
| --- | ---: | ---: |
| NVS | `0x9000` | `0x6000` |
| PHY data | `0xF000` | `0x1000` |
| Factory application | `0x10000` | `0x300000` |

There is no OTA slot. The merged delivery image starts at `0x0`. Do not switch the console to default UART0 because TX GPIO21 conflicts with the backlight.

A change that increases an LVGL buffer, audio buffer, task stack, or radio allocation reports the build memory output and measures runtime minimum free heap and largest free block. Total free heap alone does not prove that a contiguous allocation can succeed.

## Extension rules

- A reusable hardware capability adds `bsp_<feature>.h`, a BSP implementation, constants in `bsp_pins.h`, CMake dependencies, blocking/ownership documentation, and device acceptance.
- A page implements `enter`, `exit`, and `key`, is declared in `main/demo.h`, added to `main/CMakeLists.txt`, and registered in `DEMOS[]` with a defined initialization-failure state.
- Pins, addresses, panel dimensions, and resistor-derived windows are not duplicated in application code.
- State machines, protocols, timing, persistence codecs, and layout calculations that run without hardware are separated from ESP-IDF/LVGL and covered by host tests.

## Build and device acceptance

Build with ESP-IDF 5.5.3 and `./tools/validate.sh`. Flash `build/FoloToy-AI-Passport-full.bin` at `0x0`. A build result never counts as a device result.

| Subsystem | Required device observations |
| --- | --- |
| Startup/I2C | Stable USB logs; no reset loop/assert/watchdog; scan shows `0x18` and `0x63` |
| Display | Correct orientation, clipping, inversion, RGB565 byte order, and 0/25/50/75/100% backlight levels |
| Buttons | Released plus UP/DOWN/OK mV; PRESS/CLICK/DOUBLE/LONG; menu wrap and OK-long return |
| Audio | 1 kHz tone at correct speed; non-zero capture; replay speed; format change; page exit |
| Battery | Address `0x63`; SOC `0–100`; voltage displayed; I2C error isolated |
| Wi-Fi | SSID/RSSI results; rescan; ten entry/exit cycles without failure |
| Bluetooth LE | Phone sees `FoloPassport`; restart works; advertising stops on exit; ten entry/exit cycles |
| Light sleep | UP, DOWN, and OK each wake in place; timer wakes after 2 seconds; backlight and ADC buttons resume |
| Deep sleep | UP, DOWN, and OK each restart the app; timer restarts after 5 seconds; UI reports cause and retained count |
| Memory/lifecycle | Minimum free heap and largest block recorded; ten page cycles leave no tasks, timers, or objects behind |

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Backlight without image | CS/DC/MOSI/SCLK, software reset, production panel sequence, display-on command |
| Wrong color or rotation | `swap_bytes`, RGB order, inversion, LVGL rotation overriding panel mirror |
| Button misclassification | 10 kΩ pull-up, measured mV, recognition windows, `ADC_ATTEN_DB_12` |
| `adc1 is already in use` | A second ADC1 oneshot unit was created outside button BSP |
| Both I2C devices disappear | A second I2C0 bus was created or 2.2 kΩ pull-ups/3.3 V are missing |
| Audio speed or zero capture | Format close/open, 256×fs MCLK, `no_dac_ref`, DIN GPIO4, 30 dB input gain |
| Battery page fails | CW2017 `0x63`, 3.3 V I2C pull-ups, power, soldering, SOC startup delay |
| Light sleep wakes immediately | A function button remained low before sleep or GPIO0 lacks its 10 kΩ pull-up |
| Light sleep loses buttons | ADC restore or button-timer resume failed after GPIO wake |
| Deep sleep does not restart | GPIO0/timer source, boot wake cause, 10 kΩ pull-up, RTC retained counter |
| Wi-Fi/BLE second entry fails | Page exit did not stop/deinitialize the active stack |
| CJK text shows boxes | No CJK subset font or mixed-language fallback was added |

Final delivery reports `Build`, `Host tests`, `Device tests`, and `Unverified` separately.
