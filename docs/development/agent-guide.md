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
schematic / PCB / board revision / measurement
  > components/bsp/include/bsp_pins.h
  > BSP public headers and implementation
  > docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md
  > README and demo applications
```

The repository does not currently contain schematic or PCB source. Report unknown revisions, wiring, polarity, registers, and unused GPIO connections; request evidence instead of substituting another board's values.

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
- Budget internal RAM for images, fonts, networking, audio, LVGL, and task stacks; this board has no PSRAM.
- Isolate testable state machines, protocols, timing, and layout calculations from ESP-IDF/LVGL and cover them with host tests.

## Delivery

The automated gate is not hardware acceptance. Report `Build`, `Host tests`, `Device tests`, and `Unverified` separately. Use the [hardware guide](../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md) for the applicable on-device matrix.

Related documents: [build and test](build-and-test.md), [coding conventions](coding-conventions.md), [hardware guide](../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md), and [documentation index](../INDEX.md).
