<p align="right">
  <a href="coding-conventions.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Coding Conventions

- Write C with four-space indentation and K&R braces, following neighboring files. Use `snake_case`, `BSP_*` public constants, `s_` file-local state, `bsp_` public BSP APIs, and `demo_<feature>_<action>` demo entry points. Prefer `static` for internal symbols.
- Keep UI text and default documentation in English. Explanatory source comments may use Chinese while retaining established English technical terms.
- The baseline enables only LVGL Montserrat 14 and 20, which do not contain CJK glyphs. Chinese UTF-8 text therefore renders as missing-glyph boxes; changing source-file encoding does not fix it. Before adding Chinese UI text, compile and select a CJK font that covers every displayed character, prefer a glyph subset over a full font, configure a suitable fallback for mixed-language text, budget Flash and internal RAM, and verify the result on the device.
- Put reusable hardware behavior in `components/bsp`; keep menus, animations, product interaction, and validation pages in `main`.
- The `ui_pixel` theme (sky background, grass, title plate, mascot, ink-outlined panels) is part of the user interface, not a removable component. When trimming components or routing straight to a feature screen, keep the theme and build the screen through `ui_pixel_screen_create()` / `ui_pixel_panel_create()`.
- Show the battery level in the top-right corner of a user interface by default, unless the developer specifies a different placement or explicitly does not want it. Read it from `bsp_battery_soc()` (and `bsp_battery_mv()` where useful); render it as a small battery indicator or percentage in the top-right area of the screen, and degrade gracefully when it reads `-1` (unavailable). Place it where it does not overlap the existing cloud decoration (`add_cloud`, around `x≈188, y≈8`): use the clear sky space beside or below the cloud, or the very top-right edge, rather than covering the cloud.
- Document non-trivial functions, state, ownership, blocking behavior, task context, initialization order, failure values, register choices, timing, synchronization, and hardware-specific constants. Explain why, not merely what.
- Add or update tests with code changes. If automation is not practical, record the test gap and exact manual validation path.
- If adding a cache, define expiration and cleanup unless durable retention is explicitly justified.
- The ESP32-C3 has no PSRAM. Review internal RAM and largest-contiguous-block impact before increasing LVGL buffers, audio allocations, network state, or task stacks.
- **Watch power consumption.** This is a wearable powered by a small battery; keep it efficient. Avoid keeping the screen lit for long periods: dim or turn off the backlight, and return to a low-power state (light/deep sleep) whenever the screen is idle, so the device is not left displaying a bright screen while doing nothing. See the guidance on sleep in [`../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md`](../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md).
