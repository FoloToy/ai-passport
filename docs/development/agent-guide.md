<p align="right">
  <a href="agent-guide.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# AI Agent Development Guide

This guide is for AI coding assistants. `AGENTS.md` is the only mandatory starting document; read this guide for code work and route to hardware or engineering references only when the task requires them.

## Establish context

1. Read `AGENTS.md` and follow its task routing. Do not load every README or the entire hardware guide by default.
2. Run `git status --short --branch` and preserve existing changes.
3. Read affected public headers, implementations, and neighboring code. Do not infer this board's behavior from a generic ESP32-C3 board.
4. Use the current checkout as the complete implementation source. Do not fetch or depend on remote demo branches.
5. Decompose the request into inputs, outputs, state, tasks, persistence, memory budget, failure behavior, and acceptance. Ask the user before editing when a missing choice changes observable behavior.
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

A new page implements the `enter`, `exit`, and `key` interface in `main/demo_<feature>.c`, is declared in `main/demo.h`, added to `main/CMakeLists.txt`, and registered in `main.c`. Every new hardware page defines its initialization failure state in the menu.

Only reusable hardware capabilities belong in the BSP. Document blocking behavior, task context, ownership, failures, and initialization order. Pins and I2C addresses belong only in `bsp_pins.h`.

## Runtime invariants

- Hold `bsp_lvgl_lock()` whenever non-LVGL context accesses LVGL objects.
- Button callbacks dispatch lightweight events only; move audio, storage, networking, and other slow work to worker tasks.
- Before deleting a page screen, stop every task and timer that contains a code path to that page or one of its child objects.
- Preserve menu `UP`/`DOWN`, `OK` click to enter, and page `OK` long-press to return unless the change explicitly redefines them.
- Preserve the menu unless the requirement explicitly says the application starts directly. A direct-start application loads its target screen from `app_main` and keeps `ui_pixel_screen_create()` / `ui_pixel_panel_create()` unless the requirement explicitly replaces the visual system.
- Budget internal RAM for images, fonts, networking, audio, LVGL, and task stacks; this board has no PSRAM.
- Isolate testable state machines, protocols, timing, and layout calculations from ESP-IDF/LVGL and cover them with host tests.

## Material placement

Keep application-owned images, fonts, and audio under `main/assets/`; create that directory only in the same change that adds at least one tracked asset. Record source/license and register every embedding or generation step in `main/CMakeLists.txt`. Put a resource in `components/bsp` only when reusable board behavior consumes it. Do not create empty asset categories or mix binary assets with Markdown documentation.

## Delivery

The automated gate is not hardware acceptance. Report `Build`, `Host tests`, `Device tests`, and `Unverified` separately. Use the [hardware guide](../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md) for the applicable on-device matrix.

Related documents: [build and test](build-and-test.md), [coding conventions](coding-conventions.md), [hardware guide](../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md), and [documentation index](../INDEX.md).
