<p align="right">
  <a href="coding-conventions.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Coding Conventions

## C and naming

- Use four-space indentation and K&R braces.
- Use `snake_case` for functions and variables, `s_` for file-local state, `BSP_*` for public board constants, `bsp_` for public BSP APIs, and `demo_<feature>_<action>` for demo entry points.
- Declare every symbol `static` unless another translation unit consumes it.
- Keep UI strings in English. Source comments may use Chinese while retaining established English technical identifiers.

## Ownership and runtime

- Reusable board behavior, buses, drivers, and firmware constants belong in `components/bsp`; pages, application state, animations, and application tasks belong in `main`.
- Every non-LVGL task or callback locks LVGL with `bsp_lvgl_lock()` before accessing an LVGL object and unlocks every successful lock path.
- Button callbacks only enqueue or notify. Audio PCM, storage, networking, allocation, and waits run in worker tasks.
- Page exit stops and joins every task, timer, callback, and event source that can reach the page before deleting its screen.
- Each LVGL pointer keeps the type returned by its constructor. A pointer passed to `lv_label_set_text()` must come from `lv_label_create()`.

## UI, fonts, and power

- Preserve the existing `ui_pixel` visual system unless the requirement explicitly replaces it. A preserved screen uses `ui_pixel_screen_create()` and `ui_pixel_panel_create()`.
- The baseline enables Montserrat 14 and 20 only; neither contains CJK glyphs. A change that adds CJK UI text also adds a glyph-subset font covering every displayed character, configures mixed-language fallback, reports Flash/RAM cost, and includes device glyph acceptance.
- Every product application requirement defines an idle timeout, backlight behavior, and light/deep-sleep behavior. If any of these is omitted, ask the user before implementing power behavior.

## Comments, data, and resources

- Comment non-obvious ownership, blocking, task context, synchronization, failure values, register choices, timing, and hardware-specific constants. Do not comment a statement that is already self-explanatory.
- Version every persistent on-device data format before release. A format change defines migration or explicitly authorizes erasure before code is written.
- Create `main/assets/` only in a change that adds at least one tracked asset. Record source and license, register every embed/generation step, and report Flash and internal-RAM impact.
- ESP32-C3 has no PSRAM. Any increase to an LVGL buffer, audio buffer, radio state, or task stack includes the build memory report and a device measurement of minimum free heap and largest free block.

## Tests

- A change to a hardware-independent state machine, protocol, timing rule, persistence codec, or layout calculation adds or updates `tests/test_*.c` in the same change.
- A hardware-only change names the exact hardware-guide acceptance rows in the PR and reports them as `PASS`, `FAIL`, or `NOT RUN`.
- Run `./tools/run-host-tests.sh` while iterating and `./tools/validate.sh` before delivery.
