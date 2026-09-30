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

## 2a. Check resource and scheduling stress before firmware acceptance

The shared C/Wasm demo runs the game, not the device scheduler or buses. Browser
CPU throttling and an FPS cap cannot expose a higher-priority display task
busy-waiting while a lower-priority audio task misses its refill window. Keep
playability, resource stress, and physical-device acceptance as separate results.

For games with continuous audio or significant rendering/memory load, add a
repeatable stress check after Gate 2. Use
[`tools/game_resource_stress.py`](../../../tools/game_resource_stress.py) as a
small deterministic load model. It uses Python's standard library and no SDK,
USB access, or wall-clock benchmark. The repository gate runs its positive and
negative control tests; it does **not** automatically calibrate or accept a game.

1. **Export the reference workload.** Replay the accepted seed, actions and ticks
   through the actual C/Wasm build. The game's adapter exports ordered state
   changes into a trace: `schema: 1`, `duration_us`, and `events` with `at_us`,
   `audio`, and `render`. Begin at time zero. Use the actual audio gate, not the
   walking flag: a stopped player may still have ambience. Include title,
   walking, stopping, corners where applicable, outcome, return and re-entry.
   Keep the trace and Wasm/source hashes with the result. The load model does not
   implement gameplay or feed delayed execution back into that reference trace.
2. **Calibrate the load profile.** Measure representative device render CPU time,
   display wait and its blocking behavior, task priorities, audio synthesis time,
   sample/chunk rate, usable PCM buffer and free/largest-block memory. Record the
   firmware hash, scene, warm-up and sampling duration, date, measurement method
   and every assumption. Desktop Wasm time is not a device CPU measurement;
   end-to-end refresh time is not pure SPI time; descriptor capacity is not proof
   of playable PCM headroom. Where inputs remain assumptions, use `partial`,
   not `measured`. Compare model predictions with a separate device sample;
   document unexplained differences and keep calibration incomplete.
3. **Run positive and negative controls.** The candidate configuration must meet
   the project's explicit limits. Reproduce the suspected scheduling regression
   (for example, replace a blocking display wait with busy CPU occupation),
   inject non-preemptible delay longer than buffer headroom, and exceed the
   memory envelope. Those controls must fail for the intended reason; a suite
   that always passes cannot justify advancing. Put them in the game's local
   validation command, not in normal player flow.
4. **Fail closed on missing evidence.** Run with `--require-calibrated` for a
   required resource acceptance gate. Synthetic/partial inputs are useful for
   model regression and conservative experiments but do not earn a calibrated
   gate PASS. Changes to shared C, adapters, buffer sizes, task priorities or
   display behavior require refreshed traces/profiles and affected checks.

The [example profile](../../../tests/fixtures/game_resource_stress/profile.json)
and [trace](../../../tests/fixtures/game_resource_stress/trace.json) are synthetic
regression data, not hardware defaults. All timings are integer microseconds;
priorities are distinct with higher numbers scheduled first. `cpu` defines
`render_us`, `frame_period_us`, `display_wait_us`, `display_wait` (`blocking` or
`busy`), both task priorities and `audio_render_us`. `audio` defines
`sample_rate_hz`, `chunk_samples`, and `buffer_samples`. Optional ordered `stalls`
contain `at_us`/`duration_us` CPU occupation that prevents either task running;
DMA completion still advances in wall time. `memory` checks additional
`allocations` against `available_bytes`, `largest_block_bytes`, and
`reserve_bytes`, at the same baseline. Do not double-count memory already
allocated when that baseline was measured. `limits` sets maximum underrun,
startup delay, frame lateness and dropped frames. Copy and edit the example to
set these values explicitly; they are not universal board targets.

```bash
# Model regression only: the example intentionally has synthetic calibration.
python3 tools/game_resource_stress.py \
  --profile tests/fixtures/game_resource_stress/profile.json \
  --trace tests/fixtures/game_resource_stress/trace.json \
  --output /tmp/game-stress.json
python3 tests/test_game_resource_stress.py
# For the game's measured profile and actual C/Wasm reference trace:
python3 tools/game_resource_stress.py \
  --profile /path/to/game-profile.json --trace /path/to/wasm-trace.json \
  --require-calibrated --output /tmp/game-calibrated-stress.json
```

Exit codes are `0` for a passing model run, `1` for violated limits/incomplete
required calibration, and `2` for invalid inputs. The JSON report retains model
version, input hashes, calibration, failures, startup/refill interval, buffer
underrun duration, frame deadlines and memory margin. Refill interval means time
between modeled writes; it is not the firmware's synthesis-to-write feed-gap
metric. Model startup measures the first write, not codec setup or acoustic
onset. The ring starts full as an optimistic bound and synthesis writes whole
chunks; record this assumption and validate effective headroom on the device.

This is a bounded single-core, preemptive, fixed-priority model with wall-clock
transfer completion. It does not run FreeRTOS/LVGL, emulate the I2S driver,
interrupt ordering or cache/Flash effects, model equal-priority time slicing,
predict heap fragmentation, verify allocation-failure recovery, or render/audio
quality under delayed gameplay. Run application allocator fault-injection and
teardown tests separately. Real bus timing, buffering, speaker quality and
sustained load still require Gate 4. Every report says
`hardware_acceptance: NOT RUN`, even when its model limits pass.

**Exit criterion:** the current trace/profile pass the required limits, negative
controls detect their injected faults, calibration assumptions and model/device
differences are resolved or explicitly block the gate, and the exact inputs and
results are retained. When this gate is irrelevant (for example, no audio and
negligible rendering load), record `NOT APPLICABLE` with the rationale instead
of inventing audio measurements.

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
| Resource stress | Trace/profile hashes, calibration and assumptions, limits, positive/negative controls, model/device differences; `PASS` / `FAIL` / `NOT RUN` (or justified `NOT APPLICABLE`) |
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

Before handing the candidate to a device tester, provide the demo URL/start command, scenario table and demo decision, resource-stress report and calibration gaps, exact validated firmware identity, installation/data plan, and device measurement plan. Use the [hardware guide](../../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md) for applicable board checks. Web preview acceptance, firmware build acceptance, and device acceptance remain separate results.
