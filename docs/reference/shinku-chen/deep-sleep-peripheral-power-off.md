<p align="right">
  <a href="deep-sleep-peripheral-power-off.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Shutting Down On-Board Peripherals Before Deep-Sleep

This reference originated from the **"Sound Effects Keychain"** v1.4.0 firmware
on the voice-keychain edition (commit `ce9b13d`). The original device test
confirmed that the firmware slept after five idle minutes, woke on a button,
and reduced codec/panel/gauge draw. No standby-current value was measured.

The upstream BSP now uses a more defensive shutdown contract than that historic
firmware. Its automated contracts and ESP-IDF build are checked, but its exact
current reduction still requires measurement on the target board.

## Why peripheral shutdown is required

`esp_deep_sleep_start()` powers down the MCU core, but devices on the always-on
3.3 V rail remain powered. Each controllable peripheral must enter its own
low-power state before the MCU sleeps. Software suspend reduces operating blocks
and signal leakage; it does not disconnect the rail.

## Current terminal shutdown order

After configuring a wake source and stopping page-owned asynchronous work, use
this order before `esp_deep_sleep_start()`:

| Order | Interface | Result |
| ---: | --- | --- |
| 1 | `bsp_battery_sleep()` | Write CW2017 CONFIG sleep, wait 5 ms, read it back, and retry once if the write/readback does not match. |
| 2 | `bsp_audio_sleep()` | Run the complete ES8311 suspend sequence directly, verify six critical registers, retry once, and explicitly stop both I2S channels. |
| 3 | `bsp_audio_prepare_deep_sleep()` | Leave MCLK, BCLK, WS, DOUT, and DIN as inputs without internal pulls. |
| 4 | `bsp_i2c_prepare_deep_sleep()` | After both shared-bus devices finish, leave SDA/SCL as inputs without internal pulls. |
| 5 | `bsp_display_prepare_deep_sleep()` | With LVGL flushes blocked, send display-off and Sleep In, stop the backlight low, set LCD SPI safe levels, and hold them through deep sleep. |

The Wi-Fi and Bluetooth LE stacks belong to their individual demo pages and are
already stopped before the Low Power page starts. A product application with
long-lived radio services must stop those services before the LCD step.

Peripheral register failures are logged but do not prevent the remaining
shutdown steps or deep sleep. Once I2S/I2C pins have been released, the path is
terminal: if deep-sleep entry unexpectedly returns, restart instead of trying to
resume partially detached buses.

## Why `esp_codec_dev_close()` is insufficient

`esp_codec_dev_close()` only invokes the ES8311 disable path when the codec-dev
input or output was opened. A boot that never played or recorded audio can skip
that path even though the ES8311 control interface and I2S channels were already
initialized.

The BSP therefore writes the suspend registers through the independent control
interface rather than performing a silent default-format open. It also sets
REG45 to `0x01` to disable the internal BCLK/LRCK pull-ups, reads back registers
`0x00`, `0x01`, `0x0D`, `0x0E`, `0x12`, and `0x45`, and retries the full sequence
once after 5 ms. This works whether or not PCM was previously opened.

REG0E is compared through a mask rather than for equality. Bit 7 of that register
does not latch on this part: writing `0xFF` reads back as `0x7F`, and moving that
write after the REG00 reset pulse does not change it. Only bits 0-6, which the
sequence actually controls, are checked. Requiring the full `0xFF` makes every
sleep report a verification failure, and a check that always fails cannot catch
a real one - it also blocks callers that gate light sleep on this result.

## MCU pins still need an explicit terminal state

ESP-IDF isolates unheld digital GPIOs while entering deep sleep, but explicit
pin release is still useful before entry: it stops I2S clocks immediately,
removes MCU internal I2C pulls after the last transaction, and makes the
transition contract testable. External I2C pull-ups remain a hardware load.

LCD pins are the exception to floating: CS is held high, while SCLK, MOSI, DC,
and backlight are held low. On deep-sleep wake, `bsp_display_init()` releases the
global and per-pin holds before SPI and LEDC take ownership.

## What software cannot fix

`BSP_I2S_PA_CTRL = -1` means the amplifier enable is not wired to the MCU. The
amplifier's standby current, regulator quiescent current, external pull-up
current, and battery self-discharge cannot be removed by these APIs. If measured
standby draw is still high after register readback passes, isolate these hardware
loads individually.

## Wake and recovery boundary

The upstream Low Power demo uses RTC timer wake only because the shared ADC
button node has no confirmed deep-sleep wake contract in this repository. Deep
sleep restarts the application; normal BSP initialization releases LCD holds and
reinitializes display, audio, I2S, and I2C. Light sleep must not call any of the
terminal pin-release or LCD-hold interfaces and instead resumes audio with
`bsp_audio_wake()`.
