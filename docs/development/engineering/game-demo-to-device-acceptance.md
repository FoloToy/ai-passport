<p align="right">
  <a href="game-demo-to-device-acceptance.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Game demo to device acceptance SOP

Use this workflow for games that choose browser-first playtesting: compile portable C into WebAssembly (Wasm), use an HTML/CSS/JavaScript (H5) shell to tune play and interaction, then accept the resulting firmware on the device. This is a project integration pattern; the template does not supply a universal Wasm game builder or hardware emulator.

Record evidence before advancing each acceptance gate. Small device feasibility probes may run earlier, especially for a new renderer, audio path, or memory budget. Feed those measurements back into the demo; they do not replace final physical-device acceptance.

## 0. Define the acceptance contract

Before implementation, write one small scenario table in the game's task document. Include start/end states, screen orientation, the physical UP/DOWN/OK keys and their positions, short/long-press behavior, normal/failure paths, replay/exit, and the cues the player must read. Confirm the mapping on the actual board instead of assuming that screen-relative left/right means physical UP/DOWN. Set measurable device targets for frame cadence, memory margin, and input response where they matter, including the scene and sampling duration. Record which scenarios need play without debug hints and which may use controlled fixtures.

| Case | Setup and input | Expected result | Acceptance evidence |
| --- | --- | --- | --- |
| Start and pause | Start from the title; activate play, then pause | One action per press; motion stops; the next action is clear | Input test and visible play |
| Outcome and replay | Reach success or failure, then restart | Outcome remains visible until the configured action; restart resets the intended state | Controlled fixture plus a complete player run |
| Input interruption | Cancel a touch, hide the tab, or return from a long press | No stuck movement or unintended short-press action | Browser input test; physical timing checked separately |

Replace or extend these examples for the actual game. For an endless game, define a representative session and its reset/exit criteria instead of inventing a win state.

**Exit criterion:** a player can describe the intended decision and button action at each step without reading source code. Ambiguous controls or completion rules return to design, not to firmware implementation.

## 1. Build one portable C game, with an H5 review shell

Keep the gameplay state machine, clock/tick rules, movement, collision, and scoring in portable C. Compile those same source files for host tests, Wasm, and firmware. Share the renderer too when it is portable; an LVGL-based page may instead share only the game model and layout data. Put ESP-IDF/LVGL, ADC buttons, battery, display transfer, and task ownership in device adapters. Keep browser input, display, scaling, and review controls in the H5 shell, without a second scoring or movement model.

The project supplies a small bridge for initialization/reset, input events, elapsed-time ticks, state inspection, and render output where shared. Define time units, random seeds, buffer ownership, and input ordering consistently across native C and Wasm. Record the chosen compiler/SDK version and reproducible build, test, and HTTP-serve commands in the project's own guide. Load Wasm through HTTP, report load failures visibly, and keep debug fixtures out of normal player flow.

Give each game one standalone demo page. Keep cross-game navigation outside it, and document any browser-only mapping of the application exit action (for example, returning to the current game's title). That boundary is a preview convenience, not evidence that firmware navigation has been accepted.

Map the simulated controls to the agreed physical keys, including raw browser press/release and long-press timing. Normalize those raw events to the BSP-supported actions (PRESS, CLICK, DOUBLE, LONG); browser release is not a new device callback requirement. Use the selected device orientation and native pixel dimensions as the reference composition. CSS scaling may enlarge it; browser-only smoothing or animation must not conceal missing game frames or change camera motion. Keep assets, fonts, and UI copy traceable to the candidate firmware. Hash shared sources and generated artifacts, and make the project's check fail when they are stale; generating a new manifest alone is not a rebuild.

**Exit criterion:** a fresh browser load runs the current C build. Replay the same seed, input events, and tick sequence in native C and Wasm; compare states and shared render output within documented tolerances. Identify any separately implemented browser UI as a visual reference: core parity does not validate its matching LVGL page, font selection, or redraw lifecycle.

## 2. Accept the playable demo before device work

Run host logic, input, renderer, and C/Wasm parity tests. Then use the visible H5 controls for two complementary passes:

1. **Controlled pass:** exercise each success/failure branch, pause/resume where supported, interrupted actions, return to title, and replay. Use fixtures to reproduce hard-to-reach states, and save the fixture and expected outcome.
2. **Blind player pass:** start without answer-revealing controls and play the intended route end to end. Observe whether cues are discoverable, the buttons feel predictable, spatial actions leave the player oriented where applicable, transitions have no unexplained blank frame or position jump, and the win/failure feedback is unmistakable.

Review in the selected device orientation at native resolution and at a typical desktop/mobile viewport. Check title and outcome screens, text legibility, HUD timing against the world, animation continuity, and focus-loss/cancel behavior. Iterate on C, assets, or the shell and repeat affected checks until the designated player/reviewer accepts the interaction. Record reviewer, candidate, and date. A fixture-only success cannot replace normal play; a screenshot cannot establish animation comfort.

**Exit criterion:** the scenario table has PASS evidence for the complete flow and edge cases, a current C/Wasm parity result, and a recorded player decision that the demo's feel is ready for hardware. Keep open issues explicit instead of silently carrying them into the device gate.

## 3. Build and install a device candidate safely

Integrate the accepted C core into the application's own UI through the `main/` lifecycle and BSP interfaces, following the [AI development guide](../ai-guide.md). Run the [complete repository gate](build-and-test.md) in the required ESP-IDF environment. Validate the candidate's actual image offsets, application size, and partition layout using that guide; do not promote one game's partition choices into a template-wide rule. Record the commit, uncommitted-source status, Wasm manifest, firmware hash, and shared asset hashes. Retain matching build/debug artifacts as specified by the build guide.

Before installation, obtain authorization to flash and identify the actual board and serial port. Choose an installation method compatible with its current layout and data-preservation needs, following the build guide. An original-firmware readback is not a prerequisite. Device detection alone does not authorize flashing or a full-chip erase; preserve any data the user needs and respect project-specific protected regions. A successful transfer or boot proves installation, not game acceptance.

**Exit criterion:** the validated candidate boots to the intended entry screen, the installed layout matches the approved plan, and the exact installed build is identifiable.

## 4. Accept on the physical device

Repeat the same scenario table with the physical buttons. Run at least one complete normal playthrough or the agreed endless-game session, plus controlled checks for rare branches. Check initial text and glyphs, short/long-press behavior, motion, outcome/replay, exit/re-entry, and game-specific cases. Where battery is displayed, test an available reading, an unavailable placeholder, and a late reading that refreshes the screen; do not require a fabricated percentage on the first frame. Check actual redraw behavior, task cleanup, and audio when used. Compare the physical screen and hand feel with the accepted H5 reference and record differences.

Measure device-only facts in representative scenes: frame submission and, where available, panel-completion cadence; render and display time; free/minimum heap and largest block; input response; crashes, watchdogs, allocation failures, tearing, or black frames. Warm up and sample for the duration specified in the game's probe plan. Compare like-for-like scenes and use the targets defined at Gate 0. Browser performance counters, serial screenshots, and automated C tests cannot establish physical fluidity or memory safety under sustained play.

**Exit criterion:** physical controls and the full game flow pass, measured device targets pass, and no unresolved difference makes the game confusing or uncomfortable. If a fix changes shared C or assets, rebuild Wasm and firmware, rerun affected demo scenarios, then repeat the device checks; do not close a device failure with browser evidence alone.

## Record the handoff

For each candidate, keep a concise acceptance record with:

| Field | Required evidence |
| --- | --- |
| Identity | Commit, Wasm manifest/source hash, firmware and asset hashes, board revision, install method |
| Demo | Scenario table, controlled fixture results, blind-play notes, visual/motion evidence, native-C/Wasm parity |
| Build | Static/host tests, firmware and configured-layout gate results |
| Device | Installed-build proof, physical playthrough and edge cases, performance/memory sample, sanitized logs or capture |
| Decision | `PASS`, `FAIL`, or `NOT RUN` separately for Build, Host tests, Demo, and Device tests; owner, date, `Unverified` items |

Use `NOT RUN` for missing evidence. Keep sensitive device identity and unsanitized serial data out of committed records. Do not label the release ready while a required gate is `FAIL` or `NOT RUN`.

## Commands and handoff checklist

Each game must supply its own Wasm build, stale-artifact check, parity test, and HTTP preview commands. Run them before the browser acceptance pass. Once the candidate is ready, use the existing repository gates from the project root:

```bash
./tools/validate.sh --static
# Activate the required ESP-IDF environment before the complete gate.
./tools/validate.sh
```

Before handing the candidate to a device tester, provide the demo URL/start command, scenario table and demo decision, exact validated firmware identity, installation/data plan, and device measurement plan. Use the [hardware guide](../../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md) for applicable board checks. Web preview acceptance, firmware build acceptance, and device acceptance remain separate results.
