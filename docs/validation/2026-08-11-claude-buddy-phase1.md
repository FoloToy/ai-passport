# Claude Buddy phase-one validation record

Date: 2026-08-11
Repository: trae_card_bsp
Target: ESP32-C3 / FoloToy AI Passport
ESP-IDF: 5.5.3 (/home/cjiio/.espressif/v5.5.3/esp-idf)

This record reports only checks actually performed in this environment. No
FoloToy AI Passport or other usable serial device was attached, and Claude Desktop was
not available. Hardware, pairing, live-state, and soak checks are NOT RUN.

## Summary

| Area | Result | Evidence |
| --- | --- | --- |
| Host build and tests | **PASS** | Fresh CMake configure/build; 8/8 CTest tests passed |
| ESP-IDF clean ESP32-C3 build | **PASS** | fullclean, set-target esp32c3, and build exited 0 |
| Flash/partition/resource audit | **PASS** | Image, partition, DIRAM, LVGL, and PSRAM settings recorded below |
| Hardware startup and display/BLE acceptance | **NOT RUN** | No serial board detected |
| Claude Desktop pairing/reconnect | **NOT RUN** | Claude Desktop and board unavailable |
| Live permission/state safety | **NOT RUN** | Requires board and Claude Desktop |
| 20-cycle/30-minute soak | **NOT RUN** | Requires board and heap sampling |

## Step 1 — clean host and firmware build

The exact brief wrapper was attempted first:

~~~
bash -lc 'get_idf553 && cmake -S tests -B build-host && cmake --build build-host --clean-first && ctest --test-dir build-host --output-on-failure && idf.py fullclean && idf.py set-target esp32c3 && idf.py build'
~~~

It exited 127 before any build because get_idf553 is an alias in .bashrc, not
available as a non-interactive shell command. This is an environment invocation
failure, not a source/build failure.

The same requested sequence was then run with the installed ESP-IDF 5.5.3
export.sh directly:

~~~
bash -lc '. /home/cjiio/.espressif/v5.5.3/esp-idf/export.sh && cmake -S tests -B build-host && cmake --build build-host --clean-first && ctest --test-dir build-host --output-on-failure && idf.py fullclean && idf.py set-target esp32c3 && idf.py build'
~~~

Result: **PASS** (exit 0).

The final-fix wave was re-verified after all source changes with a fresh host
configure and a new clean firmware build:

~~~
bash -lc '. /home/cjiio/.espressif/v5.5.3/esp-idf/export.sh >/dev/null && cmake --fresh -S tests -B build-host && cmake --build build-host -j2 && ctest --test-dir build-host --output-on-failure'
bash -lc '. /home/cjiio/.espressif/v5.5.3/esp-idf/export.sh >/dev/null && idf.py fullclean && idf.py set-target esp32c3 && idf.py build'
~~~

Both final commands exited 0.

Host test evidence:

~~~
8/8 Test #1: buddy_state         Passed
8/8 Test #2: buddy_line          Passed
8/8 Test #3: buddy_ble           Passed
8/8 Test #4: buddy_protocol      Passed
8/8 Test #5: buddy_settings      Passed
8/8 Test #6: buddy_character     Passed
8/8 Test #7: buddy_app_logic     Passed
8/8 Test #8: buddy_orchestrator  Passed
100% tests passed, 0 tests failed out of 8
~~~

Firmware evidence:

~~~
Set Target to: esp32c3
Project build complete.
FoloToy-AI-Passport.bin binary size 0xfefd0 bytes.
Smallest app partition is 0x177000 bytes.
0x78030 bytes (32%) free.
~~~

The build output contained no project-source compiler warnings. ESP-IDF emitted
the environment notice that the component registry could not be contacted;
local managed dependencies were used and the build still completed successfully.

## Step 2 — flash, RAM, and configuration evidence

Commands:

~~~
bash -lc '. /home/cjiio/.espressif/v5.5.3/esp-idf/export.sh && idf.py size && idf.py size-components'
bash -lc '. /home/cjiio/.espressif/v5.5.3/esp-idf/export.sh && idf.py partition-table'
~~~

Both exited 0. idf.py size reported:

| Region/metric | Used | Total | Remaining |
| --- | ---: | ---: | ---: |
| Flash code | 786,424 B | — | — |
| Flash data (.rodata + appdesc) | 147,456 B | — | — |
| DIRAM (.data + .bss + text) | 173,960 B | 321,296 B | 147,336 B |
| DIRAM utilization | 54.14% | 100% | 45.86% |
| RTC SLOW | 56 B | 8,192 B | 8,136 B |
| Total image size (size tool) | 1,044,312 B | — | — |

Generated artifact sizes:

~~~
build/FoloToy-AI-Passport.bin             1,044,432 bytes (0xfefd0)
build/bootloader/bootloader.bin             21,024 bytes (0x5220)
build/partition_table/partition-table.bin   3,072 bytes
~~~

Largest archive contributions from idf.py size-components --format json:

| Archive | Total contribution | Static RAM contribution |
| --- | ---: | ---: |
| liblvgl__lvgl.a | 355,024 B | 25,164 B |
| libbt.a | 114,008 B | 1,207 B |
| libbtdm_app.a | 98,122 B | 20,811 B |
| libmain.a | 74,778 B | 37,499 B |
| libesp_app_format.a | 73,749 B | 10 B |
| libmbedcrypto.a | 57,068 B | 370 B |
| libc.a | 42,773 B | 584 B |

Partition table output:

~~~
nvs,       data,nvs,    0x9000,  24K
phy_init,  data,phy,    0xf000,   4K
factory,   app,factory, 0x10000, 1500K
~~~

Configuration checks from generated sdkconfig and sdkconfig.defaults:

~~~
CONFIG_IDF_TARGET="esp32c3"
CONFIG_ESPTOOLPY_FLASHSIZE="4MB"
CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y
CONFIG_LV_MEM_SIZE_KILOBYTES=24
CONFIG_BT_NIMBLE_SM_SC=y
CONFIG_BT_NIMBLE_SM_LEGACY is not set
CONFIG_BT_NIMBLE_NVS_PERSIST=y
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
CONFIG_SPIRAM / CONFIG_SPIRAM_*: no enabled setting found
~~~

Result: **PASS** for the reproducible build/resource audit. The ESP32-C3 has no
PSRAM and the LVGL pool remains 24 KiB (CONFIG_LV_MEM_SIZE_KILOBYTES=24).
Compared with the preceding phase-one image, the final-fix binary grew by 1,296
bytes (about 0.12% of the prior binary), while static DIRAM use decreased by
1,096 bytes. The application partition still has 0x78030 bytes (32%) free.

## Step 3 — flash and startup

Result: **NOT RUN**. Read-only device discovery found no candidates:

~~~
find /dev -maxdepth 1 \( -name 'ttyUSB*' -o -name 'ttyACM*' -o -name 'cu.*' \) -print
# no output
~~~

No idf.py flash or idf.py monitor command was run. NVS/BSP/NimBLE startup,
Claude-* advertising, sleep screen, battery indication, and reboot-loop checks
remain unverified.

## Step 4 — secure pairing and reconnect

Result: **NOT RUN**. Requires the board and Claude Desktop developer mode.
Six-digit pairing, encrypted status, bond-backed reconnect, reboot reconnect,
and device-side unpair/fresh-passkey behavior remain unverified.

## Step 5 — live states and permission safety

Result: **NOT RUN**. Zero-session sleep/idle, active busy state, transcript/recent
entries, approve-once/deny behavior, duplicate button presses, disconnect
handling, and the 30-second heartbeat timeout were not exercised on hardware.
No hardware response count or post-disconnect behavior is claimed.

## Step 6 — connection soak and heap

Result: **NOT RUN**. The required 20 connect/disconnect cycles and 30-minute
connected soak were not performed. No initial/minimum/final heap values or
watchdog, allocation, or BLE error conclusions are available.

## Step 7 — record status

The validation record is intentionally explicit about the unavailable hardware
checks above. README.md already documents the ESP-IDF export.sh fallback and the
required PASS/FAIL/NOT RUN hardware checklist, so no README correction was needed.
