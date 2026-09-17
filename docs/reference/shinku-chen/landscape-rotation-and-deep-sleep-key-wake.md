<p align="right">
  <a href="landscape-rotation-and-deep-sleep-key-wake.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Landscape Rotation and a Deep-sleep Key Wake

Collected after releasing the **Connect Four** game, which runs on a 320 × 240
landscape screen and powers itself down when idle. Both topics are about the same
board: the panel and the three-key ADC ladder.

## Rotate the panel in LVGL, not in the pages

The panel is a portrait 240 × 320 ST7789P3. The application wanted a landscape
320 × 240 logical screen, so `bsp_lvgl_set_landscape()` calls
`lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90)` after `bsp_lvgl_init()`.
The rotation itself costs nothing: `esp_lvgl_port` receives the resulting
resolution-change event and writes the panel's MADCTL (MV plus one mirror), so the
panel does the transform and no extra buffer or CPU is needed.

Two consequences are easy to miss:

- **Every page must read the current logical resolution.** The display integration
  masks the four rounded corners by writing black into the partial flush buffer.
  That mask used hard-coded `240 × 320` constants and silently blacked out a strip
  of the landscape screen; it now asks the display for its logical size, so one
  30 px radius works in both orientations.
- **The rotation direction is only observable on the device.** MV plus MX is a
  proper 90° rotation, and MV plus MY is the same rotation 180° away; which one
  reads upright depends on how the product is held. Keep the constant in one place
  with a comment that says what to change if the picture is upside down, and check
  it on hardware instead of reasoning about the datasheet.

Build the UI after the rotation, not before: layout code that runs first computes
geometry from the portrait size and stays wrong until the next rebuild.

## A low-level GPIO wake can be satisfied before the chip sleeps

The three keys share one ADC pin (GPIO0) through a resistor ladder, and it is the
only deep-sleep wake source on this board. A low-level wake was armed with
`esp_deep_sleep_enable_gpio_wakeup(1ULL << 0, ESP_GPIO_WAKEUP_GPIO_LOW)` and the
device woke up by itself within a second of every sleep attempt.

The cause is the pad, not the wake source: **while the ADC owns GPIO0 its digital
level reads 0**, even with the board's external 10 kΩ pull-up fitted. A low-level
comparator on that value is already satisfied when the chip enters deep sleep.

The fix has two parts, both in the BSP:

1. Before sleeping, release the shared ADC unit and hand the pin back as a normal
   digital input with a pull-up (`bsp_button_prepare_deep_sleep()`). The function
   returns the pin level so the caller can refuse to sleep while a key is held.
2. Log that level, and keep a marker in RTC memory so the next boot can tell a real
   deep-sleep wake from a cold start or a reset.

After the change the pad reads 1 with no key pressed, the device stays asleep, and
`esp_sleep_get_wakeup_cause()` reports the GPIO wake when a key is pressed. Note
that the ESP32-C3 has no classic RTC-IO API (`SOC_RTCIO_PIN_COUNT` is 0), so the
ordinary GPIO pull configuration is the only option.

## Press timing belongs in the board header

The button component's defaults (180 ms to count as a click, 1500 ms for a long
press) make a game feel unresponsive. The BSP now takes both values from
`bsp_pins.h` next to the voltage windows, which keeps the electrical facts and the
input feel in one reviewed place.
