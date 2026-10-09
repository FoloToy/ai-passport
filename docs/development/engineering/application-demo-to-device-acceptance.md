<p align="right">
  <a href="application-demo-to-device-acceptance.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Application demo to device acceptance SOP

Use this workflow for applications that review portable C logic and interaction in a browser before final device acceptance: timers, readers, peripheral-control interfaces, games, or other applications. Compile the shared C into WebAssembly (Wasm) and supply an HTML/CSS/JavaScript (H5) review shell. Hardware-dependent features use explicit test inputs or adapters; a browser result only validates the behavior actually represented there.

The project developer prepares the candidate and evidence, the designated reviewer accepts the interaction, and the device tester verifies the installed candidate. Advance through the gates below with recorded results. Run small device feasibility probes earlier when needed, then feed their measurements back into the preview and resource checks. Final device acceptance remains required.

This is an acceptance method. The upstream baseline supplies no universal Wasm builder, hardware emulator, or application workload simulator. Application-specific builders, fixtures, load models, thresholds, and commands belong in the application's fork or branch and are documented there.

## 0. Define the acceptance contract

Write a scenario table before implementation. Include entry and exit states, orientation, physical UP/DOWN/OK positions, supported press actions, normal/error paths, cancellation, retry, and re-entry. Confirm the mapping on the actual board; screen-relative left/right does not establish the physical button mapping. Define measurable response, update-cadence, and memory targets for the relevant workload, with warm-up and sampling duration.

| Example | Setup and input | Expected result | Evidence |
| --- | --- | --- | --- |
| Timer | Start, pause, resume, then complete | Time and feedback follow the specified rules | Deterministic clock test and normal user run |
| Reader | Open content, navigate, exit, then reopen | Position and visible content follow the retention policy | Content fixture, interaction review, and device rendering |
| Peripheral interface | Inject a delayed/unavailable reading, then recovery | Loading/error state is clear; fresh data replaces it | Adapter tests, followed by real peripheral checks |
| Input interruption | Cancel a touch, hide the tab, or release after long press | No stuck action or accidental short-press action | Browser input test and physical button timing |

Replace the examples with the application's real scenarios. Identify which need controlled fixtures and which require an ordinary end-to-end user run. For a continuously running application, define a representative session and its stop/reset criteria.

**Exit criterion:** the reviewer can describe the intended action and observable result at each step. Resolve ambiguous interaction or completion rules before using the demo as the firmware reference.

## 1. Share portable C logic with an H5 review shell

Keep testable state transitions, time rules, calculations, and protocols in portable C. Compile the same sources for host tests, Wasm, and firmware. Share rendering and layout calculations where portable; an LVGL page may share only the application model. Device adapters own ESP-IDF/LVGL integration, hardware I/O, display transfer, and tasks. The H5 shell owns browser input, presentation, and review controls without implementing a second application state machine.

Define a small bridge for initialization/reset, input events, elapsed-time ticks, injected external events, state inspection, and shared render output where applicable. Keep units, event ordering, buffer ownership, and random seeds where used consistent across targets. List every simulated external source, such as battery, sensors, network replies, or storage, and the behavior that remains unrepresented. A fixture response does not prove peripheral, radio, or persistence behavior.

Give each application one standalone demo page. Keep cross-application navigation outside it and document browser-only mappings of the application exit action, such as returning to its entry screen. The mapping is a preview convenience; firmware navigation still needs device acceptance.

Map browser press/release and long-press timing to the BSP-supported PRESS, CLICK, DOUBLE, and LONG actions used by the application. Browser release does not add a device callback requirement. Review the chosen orientation at native pixel dimensions; CSS may enlarge the view but must not hide missing updates or alter animation timing. Keep assets, fonts, and text traceable to firmware.

Record reproducible build, stale-artifact check, parity-test, and HTTP preview commands with the compiler/SDK version. Load Wasm through HTTP and show load failures. Hash shared sources and generated artifacts, and fail the project check when artifacts are stale; updating a manifest alone is not a rebuild. Keep debug fixtures outside normal user flow.

**Exit criterion:** a fresh browser load runs the current C build. Replay matching inputs, external events, and ticks in native C and Wasm and compare state and shared output within documented tolerances. Label separately implemented browser UI as a visual reference; model parity does not validate the corresponding LVGL page, fonts, or redraw lifecycle.

## 2. Accept the interaction in the demo

Run applicable host logic, input, renderer, and C/Wasm parity tests. Then use visible controls for both passes:

1. **Controlled pass:** exercise completion, errors, cancellation, retry, pause/resume where supported, exit, and re-entry. Retain fixtures and expected outcomes for rare states.
2. **Normal user pass:** complete the intended task without debug assistance. Check that information is discoverable, buttons are predictable, transitions are continuous, and success or failure is clear. For games, include play without anomaly or answer hints; other applications use their ordinary user task.

Review entry, loading, content, error, and completion states as applicable, at native resolution and a representative desktop/mobile viewport. Check text legibility, status indicators against actual state, animation continuity, and focus-loss/cancel behavior. Record reviewer, candidate, date, and open issues. A fixture-only result cannot replace a normal user run; a still image cannot establish motion comfort.

**Exit criterion:** required scenarios and parity checks pass, and the designated reviewer accepts the interaction for hardware work. Iterate on shared C, assets, or presentation until this is true.

## 3. Check resource and scheduling stress

Wasm runs portable application logic under the host's runtime. Browser CPU throttling and an FPS cap do not reproduce ESP32 scheduling, DMA, buses, memory allocation, or peripheral timing. For example, display busy-waiting can prevent an audio or sensor-service task from meeting its deadline even when the browser interaction is smooth.

Select checks from the application's risks: continuous audio, frequent display updates, sampling, communication bursts, storage stalls, or significant allocation load. Define the limits before testing. Record `NOT APPLICABLE` with a rationale for checks that do not apply; missing measurements are `NOT RUN`, not an exemption. Keep application profiles and workload adapters in the application's fork or branch; reuse a common model where applicable. The optional [general resource tool](resource-stress.md) provides periodic and buffered tasks with regression fixtures. It is not a mandatory device-performance gate and is not added to the upstream repository-wide validation script.

1. **Capture the workload.** Replay representative actions and external events through the current shared C/Wasm build, or another documented executable harness for device-only paths. Capture actual state transitions and service demand, including startup, steady work, idle, failure, teardown, and re-entry. Retain input/source/build hashes. Identify whether simulated delays affect application state or only estimate resource demand against a fixed trace.
2. **Calibrate with device measurements.** Measure relevant CPU work, blocking versus busy waits, task priorities, transfer/service intervals, usable buffer headroom, and free/minimum heap and largest block. Record board, firmware identity, scenario, warm-up, duration, method, assumptions, and uncertainty. Desktop execution time is not device CPU time; total refresh time is not pure bus time; nominal buffer capacity is not measured usable headroom. Compare predictions with a separate device sample and document discrepancies.
3. **Run positive and negative controls.** The candidate must meet its stated limits. Inject relevant faults such as a CPU stall, busy wait, delayed data, slow consumer, undersized buffer, or excessive allocation. Each control must fail for the intended reason. Cover allocation-failure recovery and teardown separately when the load model does not execute them. A suite that always passes cannot justify acceptance.
4. **Keep incomplete calibration visible.** Synthetic or partially measured inputs support regression tests and exploratory estimates. They do not earn a calibrated performance PASS. A required calibrated gate must fail or remain `NOT RUN` when evidence is missing. Refresh affected workloads, measurements, and checks after changes to shared C, adapters, buffers, priorities, or transfer behavior.

Describe model limits alongside the result: scheduling/preemption assumptions, initial buffer state, latency feedback, unmodeled interrupts, driver/cache effects, heap fragmentation, and real output quality where relevant. Memory estimates must use one consistent baseline without counting existing allocations twice. Do not present application-specific parameters as universal board defaults. Local stress results cannot certify physical-device acceptance.

**Exit criterion:** applicable limits pass, negative controls detect their faults, required calibration is supported by measurements, and unresolved model/device discrepancies are recorded as blockers. Retain exact inputs, commands/tool version, hashes, and reports so another developer can reproduce the result.

## 4. Build and install a device candidate

Integrate the accepted core into the application's own UI and lifecycle through `main/` and BSP interfaces, following the [AI development guide](../ai-guide.md). Run the [complete repository gate](build-and-test.md) in the required ESP-IDF environment and verify the candidate's actual image offsets, size, and partition layout. Record commit and uncommitted-source status, Wasm/source and asset hashes, and firmware identity. Retain matching debugging artifacts as specified by the build guide.

Obtain flashing authorization, identify the board and port, and choose the installation method under the [flashing and stored-data policy](firmware-layout.md#flashing-and-stored-data). Device detection does not authorize flashing or full-chip erasure. An original-firmware readback is not a prerequisite. Preserve needed user data and respect the application's layout. A successful upload or boot establishes installation, not application acceptance.

**Exit criterion:** the approved candidate boots to its intended entry screen, its layout matches the installation plan, and the installed build is identifiable.

## 5. Accept on the physical device

Repeat the scenario table with physical buttons, at least one complete normal user task or agreed continuous session, and controlled rare cases. Check glyphs, button timing, state feedback, redraw, exit/re-entry, and task cleanup. Where battery is displayed, verify available, unavailable, and late readings. Exercise actual peripherals, persistence, network behavior, audio, and power transitions where used; compare the screen and interaction with the accepted preview and record differences.

Measure relevant device targets under representative sustained load: input/service latency, frame submission and panel-completion cadence where observable, CPU/transfer time, free/minimum heap and largest block. Check for crashes, watchdogs, allocation failures, stale data, audio underruns, tearing, or blank frames. Use the workload, warm-up, and sample duration agreed at Gate 0. Browser counters, serial screenshots, and host tests cannot establish these device properties by themselves.

**Exit criterion:** required physical scenarios and measured targets pass, with no unresolved difference that prevents correct or comfortable use. If a fix changes shared logic or assets, rebuild Wasm and firmware, repeat affected demo checks, then repeat device checks. A browser pass cannot close a device failure.

## Record the handoff

| Field | Evidence |
| --- | --- |
| Identity | Commit and source status, Wasm/source and asset hashes, firmware hash, board revision, installation method |
| Demo | Scenario table, fixtures, normal user run, parity and visual/motion evidence, reviewer and decision |
| Resource stress | Applicable risks and limits, exact workload/profile/tool identities, calibration and assumptions, positive/negative controls, model/device differences |
| Build | Repository/host checks, firmware build and configured-layout validation, matching debug artifacts |
| Device | Installed-build identity, physical scenarios, peripheral and performance/memory measurements, sanitized evidence |
| Decision | Separate Build, Host tests, Demo, Resource stress, and Device tests results; owner, date, and `Unverified` items |

Use `PASS`, `FAIL`, or `NOT RUN`; reserve justified `NOT APPLICABLE` for optional checks outside the application's scope. Keep private device identity and unsanitized logs out of committed records. Do not declare readiness while a required gate fails or lacks evidence.

Each application documents its own preview and resource-check commands. Run those before handoff, then use the repository commands:

```bash
./tools/validate.sh --static
# Activate the required ESP-IDF environment before the complete gate.
./tools/validate.sh
```

Give the device tester the preview URL/start command, accepted scenarios and open issues, resource evidence, exact validated firmware identity, installation/data plan, and measurement plan. Use the [hardware guide](../../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md) for applicable board checks.
