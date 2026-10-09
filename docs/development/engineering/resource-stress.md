<p align="right">
  <a href="resource-stress.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# General resource-stress experiments

Use this optional tool at the resource stage of the
[application demo-to-device SOP](application-demo-to-device-acceptance.md).
[`tools/resource_stress.py`](../../../tools/resource_stress.py) runs a deterministic,
single-core load model for application-defined tasks. Games, readers, recorders,
and other applications use the same engine. Application adapters supply task
activation traces and profiles; they do not implement separate schedulers.

## Run and accept a workload

```bash
python3 tests/test_resource_stress.py
python3 tools/resource_stress.py \
  --profile tests/fixtures/resource_stress/reader-profile.json \
  --trace tests/fixtures/resource_stress/reader-trace.json \
  --output /tmp/reader-resource-report.json
python3 tools/resource_stress.py \
  --profile tests/fixtures/resource_stress/recorder-profile.json \
  --trace tests/fixtures/resource_stress/recorder-trace.json \
  --output /tmp/recorder-resource-report.json
```

Both fixtures are synthetic, not measured implementations of baseline demos.
The reader has a display and buffered playback; the recorder has sampling,
storage and status refresh with no audio dependency. Tests also exercise an
isolated periodic task and two independent buffered streams. A separate,
1-microsecond reference scheduler checks 150 seeded mixed workloads against
the event-driven engine. Regression cases include busy waits, preemption, stalls,
empty/full buffers, deadline overruns, cancellation, re-entry, memory failures,
and refusal to accept incomplete calibration.

Applications can export task-demand traces from their shared C/Wasm core or
another executable harness, then apply this same engine. Keep application state,
assets, calibration profiles and adapters in their own forks or branches.
Synthetic examples validate the model; they are not measured integrations of
upstream peripheral demos. Such integrations and device calibration remain open.

This tool is optional and is not added to the upstream `tools/validate.sh` gate.
A path-filtered `resource-stress.yml` workflow runs only its host regressions when
its source, tests, fixtures, guide or workflow changes. It does not certify device
performance and should not be made an unconditional required check.

## Profile and trace contract (schema 2)

A profile contains `tasks`, `memory`, optional `stalls`, and `calibration`.
All times are integer microseconds derived from device measurements or explicitly
labeled assumptions. Host execution speed never sets device task costs.

| Task fields | Meaning |
| --- | --- |
| `id`, `priority`, `cpu_us`, `mode`, `limits` | Unique name, unique fixed priority (larger wins), CPU work per job, task mode, acceptance limits |
| Periodic: `period_us`, `transfer_us`, `wait` | Release interval, subsequent wall-clock transfer duration, `blocking` or `busy` wait |
| Buffered: `refill_us`, `capacity_us`, `initial_buffer_us` | Service duration added by each completed job, buffer capacity and explicit prefill on activation |
| Periodic limits | `max_lateness_us`, `max_dropped_jobs` |
| Buffered limits | `max_underrun_us`, `max_startup_us` |

A periodic task has one outstanding job. Releases while CPU work or transfer is
pending are dropped; no unbounded backlog is implied. Its deadline is the release
time plus its period, including transfer time. Incomplete overdue work at exit or
the end of the trace still contributes to lateness. Blocking transfers free the
CPU; busy waits compete at task priority. Transfers progress during CPU stalls.

A buffered task replenishes a continuously consumed buffer and waits for space
between writes. Use duration of service headroom, not bytes: for PCM, convert
samples using the actual sample rate. Initial prefill is explicit, refills saturate
at capacity, and the consumer starts draining immediately on activation. Startup
is time to the first producer completion; a full buffer does not hide a missing
first write. This model assumes a fixed consumption rate and whole-job writes.
Do not use it to claim packet latency, variable-rate consumption or queue overflow.

A trace contains `schema: 2`, `duration_us`, and ordered `events`. Each event has
`at_us` and `active`, the complete set of enabled task IDs. The first event is at
zero. An empty set means idle; unchanged sets do not reset tasks. Deactivation
cancels outstanding work. Reactivation releases a job immediately and resets any
buffer to its declared prefill. Completions at an event boundary are accounted
before that event; new releases follow the event. Extra fields can preserve
application states, input identifiers and source/artifact hashes.

`stalls` contain ordered, non-overlapping `at_us`/`duration_us` intervals during
which no task gets CPU service. Inputs are limited to 32 tasks with distinct
priorities, 10,000 events/stalls, 600 seconds and 200,000 estimated releases.
Equal-priority time slicing is unsupported and rejected.

`memory` supplies `available_bytes`, `largest_block_bytes`, `reserve_bytes` and
named additional `allocations`. All values refer to one captured baseline.
The engine subtracts only additional allocations, checks the remaining reserve,
and compares each request with the observed largest block. This is an envelope,
not an allocator: multiple requests can fragment memory despite passing it.
There is no built-in board memory size, display format or audio sample rate.

## Reports, calibration and boundaries

Reports include per-task completion/drop/cancellation counts, deadline lateness,
first-write delay, refill interval, underrun duration and minimum buffered headroom.
Non-applicable counters are zero; an unused buffer minimum is `null`.
Global metrics include occupied CPU time (including stalls and busy waits) and
remaining memory. CLI reports hash the model, profile and trace for reproduction.
Exit codes are `0` for modeled limits passed, `1` for a limit/calibration failure,
and `2` for invalid inputs or I/O errors.

`calibration` records `status` (`synthetic`, `partial`, or `measured`), `source`
and `assumptions`. Measured profiles additionally require `date`, a
`firmware_sha256`, and no unmeasured assumptions. These metadata checks do not
verify the measurements: review their evidence and compare a separate device
sample using the SOP. A measured label alone is not independent calibration.

Run the same command with `--require-calibrated` when a calibrated gate is
required. Synthetic or partial input then fails even if modeled deadlines pass.
Reports always keep `hardware_acceptance: NOT RUN`; partial/synthetic reports
also keep `calibrated_resource_acceptance: NOT RUN`. Retain failing reports as
well as the positive candidate, and verify each injected fault's failure reason.

The model does not execute FreeRTOS, interrupts, shared bus contention, locks,
SPI/I2S drivers, caches, Flash stalls unless explicitly injected, or heap
fragmentation. Modeled delays do not feed back into C/Wasm application state.
Test failure recovery and cleanup through executable application tests; use
physical devices for sustained timing, output quality and final acceptance.
