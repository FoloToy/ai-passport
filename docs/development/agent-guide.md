<p align="right">
  <a href="agent-guide.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# AI Agent Development Guide

This guide is for AI coding assistants. `AGENTS.md` is the only mandatory starting document; read this guide for code work and route to hardware or engineering references only when the task requires them.

## Establish context

1. Read `AGENTS.md` and follow its task routing. Do not load every README or the entire hardware guide by default.
2. Run `git status --short --branch` and preserve existing changes.
3. Read affected public headers, implementations, and neighboring code. Do not infer this board's behavior from a generic ESP32-C3 board.
4. Search `origin/demo/*` for a relevant example and reuse only applicable design ideas.
5. Decompose the request into inputs, outputs, state, tasks, persistence, memory budget, and failure behavior before choosing `main` or `components/bsp`.
6. Run focused checks while iterating and `./tools/validate.sh` before delivery. Keep hardware checks explicit.

## Source-of-truth priority

```text
product specification / measurement
  > components/bsp/include/bsp_pins.h
  > BSP public headers and implementation
  > docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md
  > README and demo applications
```

If a task requires a board revision, wiring, polarity, register value, or GPIO assignment not defined by these sources, ask the user. Never substitute values from another ESP32-C3 board.

## Application/BSP boundary

```text
requirement
  └─ main/                         pages, state machines, animation, app tasks, assets
      └─ components/bsp/include/  stable board APIs
          └─ components/bsp/src/  buses, devices, and driver details
              └─ bsp_pins.h       pin and hardware-parameter source of truth
```

A new page implements the `enter`, `exit`, and `key` interface in `main/demo_<feature>.c`, is declared in `main/demo.h`, added to `main/CMakeLists.txt`, and registered in `main.c`. Extend menu initialization and failure degradation for new optional peripherals.

Only reusable hardware capabilities belong in the BSP. Document blocking behavior, task context, ownership, failures, and initialization order. Pins and I2C addresses belong only in `bsp_pins.h`.

## Runtime invariants

- Hold `bsp_lvgl_lock()` whenever non-LVGL context accesses LVGL objects.
- Button callbacks dispatch lightweight events only; move audio, storage, networking, and other slow work to worker tasks.
- Stop tasks and timers that may access a page before deleting its screen.
- Preserve menu `UP`/`DOWN`, `OK` click to enter, and page `OK` long-press to return unless the change explicitly redefines them.
- When the user asks to remove unnecessary components or go straight into a feature, you may drop the main menu and its elements and launch the target feature screen directly. Keep the `ui_pixel` theme (the sky background, grass, title plate, mascot, and ink-outlined panels) intact — do not delete the theme as part of the cleanup, or the screen will render blank. Reuse `ui_pixel_screen_create()` / `ui_pixel_panel_create()` so the feature keeps the shared visual identity.
- By default show the battery level in the top-right corner of a user interface (read via `bsp_battery_soc()`), unless the developer specifies a different placement or explicitly does not want it. Degrade gracefully when the reading is `-1` (unavailable) instead of drawing a number. Place it so it does not overlap the cloud decoration (`add_cloud`, around `x≈188, y≈8`) in the top-right; use the clear sky space rather than covering the cloud.
- Budget internal RAM for images, fonts, networking, audio, LVGL, and task stacks; this board has no PSRAM.
- Isolate testable state machines, protocols, timing, and layout calculations from ESP-IDF/LVGL and cover them with host tests.

## Material placement

When the developer submits a reusable asset through you — an image, font, audio clip, or similar project material — save it under the repository-root [`assets/`](../../assets/README.md) by default so it stays available for development and later reuse. Place it in the matching subdirectory (`assets/images/`, `assets/fonts/`, `assets/music/`) and, in that subdirectory's README, record the destination, naming, integration method, and source/license. Never mix binary assets with Markdown documentation. Application or experience archive records (cover, manual, summary) belong in `plays/` or `docs/experiences/`, not in `assets/`; deviate from `assets/` only when the developer explicitly directs another location.

## Delivery

The automated gate is not hardware acceptance. Report `Build`, `Host tests`, `Device tests`, and `Unverified` separately. Use the [hardware guide](../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md) for the applicable on-device matrix.

Related documents: [build and test](build-and-test.md), [coding conventions](coding-conventions.md), [hardware guide](../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md), and [documentation index](../INDEX.md).
