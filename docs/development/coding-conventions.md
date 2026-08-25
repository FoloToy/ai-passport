<p align="right">
  <a href="coding-conventions.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Coding Conventions

- Write C with four-space indentation and K&R braces, following neighboring files. Use `snake_case`, `BSP_*` public constants, `s_` file-local state, `bsp_` public BSP APIs, and `demo_<feature>_<action>` demo entry points. Prefer `static` for internal symbols.
- Keep UI text and default documentation in English. Explanatory source comments may use Chinese while retaining established English technical terms.
- Put reusable hardware behavior in `components/bsp`; keep menus, animations, product interaction, and validation pages in `main`.
- Document non-trivial functions, state, ownership, blocking behavior, task context, initialization order, failure values, register choices, timing, synchronization, and hardware-specific constants. Explain why, not merely what.
- Add or update tests with code changes. If automation is not practical, record the test gap and exact manual validation path.
- If adding a cache, define expiration and cleanup unless durable retention is explicitly justified.
- The ESP32-C3 has no PSRAM. Review internal RAM and largest-contiguous-block impact before increasing LVGL buffers, audio allocations, network state, or task stacks.
