#!/usr/bin/env python3
"""Deterministic single-core load model; not a board or RTOS emulator.

All durations are integer microseconds. A project supplies device timings and a
reference workload exported by its C/Wasm adapter. No host CPU timings are used.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys

MODEL_VERSION = 1


def integer(obj, key, low=0, high=600_000_000):
    value = obj[key]
    if type(value) is not int or not low <= value <= high:
        raise ValueError(f"{key} must be an integer in [{low}, {high}]")
    return value


def validate(profile, trace):
    if not isinstance(profile, dict) or not isinstance(trace, dict):
        raise ValueError("profile and trace must be JSON objects")
    if profile.get("schema") != 1 or trace.get("schema") != 1:
        raise ValueError("profile and trace schema must be 1")
    duration = integer(trace, "duration_us", 1)
    cpu, audio, memory = (profile[key] for key in ("cpu", "audio", "memory"))
    for key in ("render_us", "frame_period_us", "audio_render_us"):
        integer(cpu, key, 1)
    integer(cpu, "display_wait_us")
    for key in ("render_priority", "audio_priority"):
        integer(cpu, key, 0, 31)
    if cpu["render_priority"] == cpu["audio_priority"]:
        raise ValueError("equal priorities/time slicing are outside this model")
    if cpu["display_wait"] not in ("blocking", "busy"):
        raise ValueError("display_wait must be blocking or busy")
    rate = integer(audio, "sample_rate_hz", 1, 192_000)
    chunk = integer(audio, "chunk_samples", 1, 192_000)
    buffer = integer(audio, "buffer_samples", chunk, 1_920_000)
    period = chunk * 1_000_000 // rate
    if period * rate != chunk * 1_000_000:
        raise ValueError("chunk duration must be an integer number of microseconds")
    if buffer * 1_000_000 % rate:
        raise ValueError("buffer duration must be an integer number of microseconds")
    if duration // period > 200_000 or duration // cpu["frame_period_us"] > 200_000:
        raise ValueError("trace exceeds the 200000-release limit")
    for key in ("available_bytes", "largest_block_bytes", "reserve_bytes"):
        integer(memory, key, 0, 8 * 1024 * 1024)
    if memory["largest_block_bytes"] > memory["available_bytes"]:
        raise ValueError("largest block cannot exceed total available memory")
    names = set()
    for allocation in memory["allocations"]:
        integer(allocation, "bytes", 1, 8 * 1024 * 1024)
        name = allocation["name"]
        if not isinstance(name, str) or not name or name in names:
            raise ValueError("allocations require unique nonempty names")
        names.add(name)
    limits = profile["limits"]
    for key in ("max_audio_underrun_us", "max_startup_us", "max_frame_lateness_us",
                "max_dropped_frames"):
        integer(limits, key)
    events = trace["events"]
    if not events or events[0]["at_us"] != 0:
        raise ValueError("trace must define the initial state at time zero")
    previous = -1
    for event in events:
        at = integer(event, "at_us", 0, duration - 1)
        if at <= previous:
            raise ValueError("trace events must be strictly increasing")
        if type(event["audio"]) is not bool or type(event["render"]) is not bool:
            raise ValueError("trace audio and render values must be booleans")
        previous = at
    previous_end = 0
    for stall in profile.get("stalls", []):
        at = integer(stall, "at_us", 0, duration - 1)
        length = integer(stall, "duration_us", 1, duration)
        if at < previous_end or at + length > duration:
            raise ValueError("CPU stalls must be ordered, disjoint and inside the trace")
        previous_end = at + length
    calibration = profile["calibration"]
    if calibration["status"] not in ("synthetic", "partial", "measured"):
        raise ValueError("calibration status must be synthetic, partial or measured")
    if not isinstance(calibration["source"], str) or not calibration["source"]:
        raise ValueError("calibration requires a source description")
    if not isinstance(calibration["assumptions"], list):
        raise ValueError("calibration assumptions must be a list")
    if calibration["status"] == "measured":
        digest = calibration.get("firmware_sha256", "")
        if len(digest) != 64 or any(c not in "0123456789abcdef" for c in digest):
            raise ValueError("measured calibration requires a firmware SHA-256")
        if not calibration.get("date") or calibration["assumptions"]:
            raise ValueError("measured calibration requires a date and no unmeasured inputs")
    return period


def simulate(profile, trace):
    """Preemptive fixed priorities, wall-clock DMA completion and PCM drain.

    Audio has one pending synthesis job and blocks when the ring has less than
    one chunk of space. It can catch up after a delay without an unbounded job
    queue. Frames do not accumulate. Buffer starts full at activation (optimistic prefill assumption).
    The separately checked first-write delay prevents that prefill from hiding
    missing startup. Pausing movement need not disable ambient audio.
    """
    audio_period = validate(profile, trace)
    cpu, audio = profile["cpu"], profile["audio"]
    end = trace["duration_us"]
    capacity = audio["buffer_samples"] * 1_000_000 // audio["sample_rate_hz"]
    metrics = dict(audio_underrun_us=0.0, max_refill_interval_us=0,
                   max_startup_us=0, audio_writes=0,
                   frames_completed=0, dropped_frames=0, max_frame_lateness_us=0,
                   cpu_busy_us=0, min_buffer_us=capacity)
    now = event_index = 0
    audio_on = render_on = False
    next_audio = next_frame = end
    pcm = 0.0
    audio_remaining = None
    render_remaining = None
    display_end = None
    frame_deadline = None
    activation = None
    first_write = False
    last_write = None
    events = trace["events"]
    stalls = profile.get("stalls", [])
    stall_boundaries = sorted({v for s in stalls for v in (s["at_us"], s["at_us"] + s["duration_us"])})

    def finish_frame():
        nonlocal render_remaining, display_end, frame_deadline
        metrics["frames_completed"] += 1
        metrics["max_frame_lateness_us"] = max(metrics["max_frame_lateness_us"],
                                                   max(0, now - frame_deadline))
        render_remaining = display_end = frame_deadline = None

    while now < end:
        if display_end is not None and display_end <= now:
            finish_frame()  # DMA completion is a wall-clock event, not a CPU job.
        if event_index < len(events) and events[event_index]["at_us"] == now:
            event = events[event_index]
            if event["audio"] != audio_on:
                audio_on = event["audio"]
                audio_remaining = None
                last_write = None
                if audio_on:
                    activation, first_write = now, False
                    pcm, next_audio = capacity, now
                else:
                    if not first_write and activation is not None:
                        metrics["max_startup_us"] = max(metrics["max_startup_us"], now - activation)
                    activation = None
                    pcm, next_audio = 0.0, end
            if event["render"] != render_on:
                render_on = event["render"]
                render_remaining = display_end = frame_deadline = None
                next_frame = now if render_on else end
            event_index += 1
        if audio_on and now == next_audio:
            audio_remaining = cpu["audio_render_us"]
            next_audio = end
        if render_on and now == next_frame:
            if render_remaining is None and display_end is None:
                render_remaining = cpu["render_us"]
                frame_deadline = now + cpu["frame_period_us"]
            else:
                metrics["dropped_frames"] += 1
            next_frame += cpu["frame_period_us"]

        locked = any(s["at_us"] <= now < s["at_us"] + s["duration_us"] for s in stalls)
        candidates = []
        if audio_remaining is not None:
            candidates.append((cpu["audio_priority"], "audio"))
        if render_remaining is not None or (display_end is not None and cpu["display_wait"] == "busy"):
            candidates.append((cpu["render_priority"], "render"))
        task = "stall" if locked else max(candidates)[1] if candidates else "idle"
        boundaries = [end, next_audio, next_frame]
        if event_index < len(events):
            boundaries.append(events[event_index]["at_us"])
        boundaries.extend(t for t in stall_boundaries if t > now)
        if display_end is not None:
            boundaries.append(display_end)
        if task == "audio":
            boundaries.append(now + audio_remaining)
        elif task == "render" and render_remaining is not None:
            boundaries.append(now + render_remaining)
        until = min(t for t in boundaries if t > now)
        delta = until - now
        if audio_on:
            metrics["audio_underrun_us"] += max(0.0, delta - pcm)
            pcm = max(0.0, pcm - delta)
            metrics["min_buffer_us"] = min(metrics["min_buffer_us"], pcm)
        if task != "idle":
            metrics["cpu_busy_us"] += delta
        if task == "audio":
            audio_remaining -= delta
        elif task == "render" and render_remaining is not None:
            render_remaining -= delta
        now = until
        if audio_remaining == 0:
            audio_remaining = None
            if not first_write:
                metrics["max_startup_us"] = max(metrics["max_startup_us"], now - activation)
                first_write = True
            if last_write is not None:
                metrics["max_refill_interval_us"] = max(metrics["max_refill_interval_us"], now - last_write)
            last_write = now
            metrics["audio_writes"] += 1
            pcm = min(capacity, pcm + audio_period)
            next_audio = now + max(0, int(pcm - (capacity - audio_period)))
        if render_remaining == 0:
            render_remaining = None
            display_end = now + cpu["display_wait_us"]
            if display_end == now:
                finish_frame()
    if display_end is not None and display_end <= end:
        finish_frame()
    if audio_on and not first_write:
        metrics["max_startup_us"] = max(metrics["max_startup_us"], end - activation)
    if frame_deadline is not None:
        metrics["max_frame_lateness_us"] = max(metrics["max_frame_lateness_us"], max(0, end - frame_deadline))
    metrics["audio_underrun_us"] = round(metrics["audio_underrun_us"], 3)
    metrics["min_buffer_us"] = round(metrics["min_buffer_us"], 3)
    metrics["cpu_busy_percent"] = round(100 * metrics["cpu_busy_us"] / end, 3)

    failures = []
    for metric in ("audio_underrun_us", "max_startup_us", "max_frame_lateness_us", "dropped_frames"):
        limit_key = metric if metric.startswith("max_") else "max_" + metric
        if metrics[metric] > profile["limits"][limit_key]:
            failures.append(f"{metric}: {metrics[metric]} > {profile['limits'][limit_key]}")
    memory = profile["memory"]
    requested = sum(a["bytes"] for a in memory["allocations"])
    metrics["memory_remaining_bytes"] = memory["available_bytes"] - requested
    if metrics["memory_remaining_bytes"] < memory["reserve_bytes"]:
        failures.append("allocation envelope consumes the reserved memory margin")
    for allocation in memory["allocations"]:
        if allocation["bytes"] > memory["largest_block_bytes"]:
            failures.append(f"allocation {allocation['name']} exceeds the observed largest block")
    return dict(model_version=MODEL_VERSION, result="FAIL" if failures else "PASS",
                calibration=profile["calibration"], metrics=metrics, failures=failures,
                hardware_acceptance="NOT RUN")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", type=Path, required=True)
    parser.add_argument("--trace", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--require-calibrated", action="store_true",
                        help="fail when any load input remains synthetic or assumed")
    args = parser.parse_args(argv)
    try:
        profile_bytes, trace_bytes = args.profile.read_bytes(), args.trace.read_bytes()
        report = simulate(json.loads(profile_bytes), json.loads(trace_bytes))
        report.update(profile_sha256=hashlib.sha256(profile_bytes).hexdigest(),
                      trace_sha256=hashlib.sha256(trace_bytes).hexdigest())
        if args.require_calibrated and report["calibration"]["status"] != "measured":
            report["result"] = "FAIL"
            report["failures"].append("device calibration is incomplete; this is not a release gate PASS")
        output = json.dumps(report, indent=2) + "\n"
        if args.output:
            args.output.write_text(output, encoding="utf-8")
        print(output, end="")
        return 0 if report["result"] == "PASS" else 1
    except (ValueError, KeyError, TypeError, OSError) as error:
        print(f"Invalid stress input: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
