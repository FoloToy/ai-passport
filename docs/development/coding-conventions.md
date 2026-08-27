<p align="right">
  <a href="coding-conventions.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Coding Conventions

- Write C with four-space indentation and K&R braces, following neighboring files. Use `snake_case`, `BSP_*` public constants, `s_` file-local state, `bsp_` public BSP APIs, and `demo_<feature>_<action>` demo entry points. Prefer `static` for internal symbols.
- Keep UI text and default documentation in English. Explanatory source comments may use Chinese while retaining established English technical terms.
- The baseline enables only LVGL Montserrat 14 and 20, which do not contain CJK glyphs. Chinese UTF-8 text therefore renders as missing-glyph boxes; changing source-file encoding does not fix it. Before adding Chinese UI text, compile and select a CJK font that covers every displayed character, prefer a glyph subset over a full font, configure a suitable fallback for mixed-language text, budget Flash and internal RAM, and verify the result on the device.
- Put reusable hardware behavior in `components/bsp`; keep menus, animations, product interaction, and validation pages in `main`.
- Document non-trivial functions, state, ownership, blocking behavior, task context, initialization order, failure values, register choices, timing, synchronization, and hardware-specific constants. Explain why, not merely what.
- Add or update tests with code changes. If automation is not practical, record the test gap and exact manual validation path.
- If adding a cache, define expiration and cleanup unless durable retention is explicitly justified.
- The ESP32-C3 has no PSRAM. Review internal RAM and largest-contiguous-block impact before increasing LVGL buffers, audio allocations, network state, or task stacks.
