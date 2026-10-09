#!/usr/bin/env python3
"""Deterministic resource experiment for application-defined tasks.

Not a board/RTOS emulator. Inputs use integer microseconds and measured or
explicitly assumed device costs, never desktop wall-clock benchmarks.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

MODEL_VERSION = 2
MAX_RELEASES = 200_000


def integer(obj, key, low=0, high=600_000_000):
    value = obj[key]
    if type(value) is not int or not low <= value <= high:
        raise ValueError(f'{key} must be an integer in [{low}, {high}]')
    return value


def mapping(value, name):
    if not isinstance(value, dict):
        raise ValueError(f'{name} must be an object')
    return value


def sequence(value, name, maximum, minimum=0):
    if not isinstance(value, list) or not minimum <= len(value) <= maximum:
        raise ValueError(f'{name} must be a list with {minimum}..{maximum} entries')
    return value


def validate(profile, trace):
    mapping(profile, 'profile'); mapping(trace, 'trace')
    if type(profile.get('schema')) is not int or profile['schema'] != 2 or type(trace.get('schema')) is not int or trace['schema'] != 2:
        raise ValueError('profile and trace schema must be 2')
    duration = integer(trace, 'duration_us', 1)
    tasks = sequence(profile['tasks'], 'tasks', 32, 1)
    ids, priorities, releases = set(), set(), 0
    for task in tasks:
        mapping(task, 'task')
        name = task['id']
        if not isinstance(name, str) or not re.fullmatch(r'[a-zA-Z][a-zA-Z0-9_-]{0,63}', name) or name in ids:
            raise ValueError('tasks require unique identifier names')
        ids.add(name)
        priority = integer(task, 'priority', 0, 31)
        if priority in priorities:
            raise ValueError('equal priorities/time slicing are outside this model')
        priorities.add(priority)
        integer(task, 'cpu_us', 1)
        limits = mapping(task['limits'], 'task limits')
        if task['mode'] == 'periodic':
            period = integer(task, 'period_us', 1)
            integer(task, 'transfer_us')
            if task['wait'] not in ('blocking', 'busy'):
                raise ValueError('wait must be blocking or busy')
            keys = {'max_lateness_us', 'max_dropped_jobs'}
        elif task['mode'] == 'buffered':
            period = integer(task, 'refill_us', 1)
            capacity = integer(task, 'capacity_us', period)
            integer(task, 'initial_buffer_us', 0, capacity)
            keys = {'max_underrun_us', 'max_startup_us'}
        else:
            raise ValueError('mode must be periodic or buffered')
        if set(limits) != keys:
            raise ValueError(f'{name} limits must be {sorted(keys)}')
        for key in keys:
            integer(limits, key)
        releases += duration // period + 1
    events = sequence(trace['events'], 'events', 10_000, 1)
    # Each reactivation can release an extra job, independently of its period.
    if releases + len(events) * len(tasks) > MAX_RELEASES:
        raise ValueError('trace exceeds the 200000-release budget')
    if events[0]['at_us'] != 0:
        raise ValueError('trace must define its initial active tasks at time zero')
    previous = -1
    for event in events:
        mapping(event, 'event')
        at = integer(event, 'at_us', 0, duration - 1)
        if at <= previous:
            raise ValueError('trace events must be strictly increasing')
        active = sequence(event['active'], 'active', len(tasks))
        if any(not isinstance(name, str) for name in active) or len(set(active)) != len(active) or not set(active) <= ids:
            raise ValueError('active must contain unique known task IDs')
        previous = at
    previous_end = 0
    for stall in sequence(profile.get('stalls', []), 'stalls', 10_000):
        mapping(stall, 'stall')
        at = integer(stall, 'at_us', 0, duration - 1)
        length = integer(stall, 'duration_us', 1, duration)
        if at < previous_end or at + length > duration:
            raise ValueError('CPU stalls must be ordered, disjoint and inside the trace')
        previous_end = at + length
    memory = mapping(profile['memory'], 'memory')
    for key in ('available_bytes', 'largest_block_bytes', 'reserve_bytes'):
        integer(memory, key, 0, 2**53 - 1)
    if memory['largest_block_bytes'] > memory['available_bytes']:
        raise ValueError('largest block cannot exceed total available memory')
    names = set()
    for allocation in sequence(memory['allocations'], 'allocations', 1024):
        mapping(allocation, 'allocation'); integer(allocation, 'bytes', 1, 2**53 - 1)
        name = allocation['name']
        if not isinstance(name, str) or not name or name in names:
            raise ValueError('allocations require unique nonempty names')
        names.add(name)
    calibration = mapping(profile['calibration'], 'calibration')
    if calibration['status'] not in ('synthetic', 'partial', 'measured'):
        raise ValueError('calibration status must be synthetic, partial or measured')
    if not isinstance(calibration['source'], str) or not calibration['source']:
        raise ValueError('calibration requires a source description')
    assumptions = sequence(calibration['assumptions'], 'assumptions', 1024)
    if any(not isinstance(x, str) or not x for x in assumptions):
        raise ValueError('assumptions must contain nonempty descriptions')
    if calibration['status'] == 'measured':
        if not re.fullmatch(r'[0-9a-f]{64}', calibration.get('firmware_sha256', '')):
            raise ValueError('measured calibration requires a firmware SHA-256')
        if not calibration.get('date') or assumptions:
            raise ValueError('measured calibration requires a date and no unmeasured inputs')


def simulate(profile, trace, *, require_calibrated=False):
    """Single core, unique fixed priorities, one pending job per task.

    Periodic jobs drop releases while work/transfer remains pending. Buffered
    producers replenish a continuously consumed buffer and block for space.
    Transfer completion is wall-clock time, including during CPU stalls.
    Trace activation cancels work on deactivation; no application state is run
    here, and estimated delays are not fed back into the caller's reference.
    """
    validate(profile, trace)
    end, now, event_index = trace['duration_us'], 0, 0
    specs = {t['id']: t for t in profile['tasks']}
    states = {name: dict(active=False, remaining=None, transfer_end=None,
                        deadline=None, next_release=end, buffer=0,
                        activation=None, first_write=False, last_write=None)
              for name in specs}
    metrics = {name: dict(completed=0, dropped_jobs=0, cancelled_jobs=0,
                          max_lateness_us=0, max_startup_us=0,
                          max_refill_interval_us=0, underrun_us=0,
                          min_buffer_us=None) for name in specs}
    events, stalls = trace['events'], profile.get('stalls', [])
    boundaries = sorted({v for s in stalls for v in (s['at_us'], s['at_us'] + s['duration_us'])})
    stall_index = boundary_index = cpu_busy = 0

    def close_pending(name):
        s, m = states[name], metrics[name]
        if s['deadline'] is not None:
            m['max_lateness_us'] = max(m['max_lateness_us'], now - s['deadline'])
        if s['activation'] is not None and not s['first_write']:
            m['max_startup_us'] = max(m['max_startup_us'], now - s['activation'])

    def finish_periodic(name):
        s, m = states[name], metrics[name]
        m['completed'] += 1
        m['max_lateness_us'] = max(m['max_lateness_us'], now - s['deadline'])
        s['remaining'] = s['transfer_end'] = s['deadline'] = None

    while now < end:
        for name, s in states.items():
            if s['transfer_end'] is not None and s['transfer_end'] <= now:
                finish_periodic(name)
        if event_index < len(events) and events[event_index]['at_us'] == now:
            active = set(events[event_index]['active'])
            for name, s in states.items():
                enabled = name in active
                if enabled == s['active']:
                    continue
                close_pending(name)
                if s['remaining'] is not None or s['transfer_end'] is not None:
                    metrics[name]['cancelled_jobs'] += 1
                s.update(active=enabled, remaining=None, transfer_end=None,
                         deadline=None, next_release=now if enabled else end,
                         activation=None, first_write=False, last_write=None)
                if specs[name]['mode'] == 'buffered':
                    s['buffer'] = specs[name]['initial_buffer_us'] if enabled else 0
                    s['activation'] = now if enabled else None
                    if enabled:
                        old = metrics[name]['min_buffer_us']
                        metrics[name]['min_buffer_us'] = s['buffer'] if old is None else min(old, s['buffer'])
            event_index += 1
        for name, s in states.items():
            if not s['active'] or s['next_release'] != now:
                continue
            t, m = specs[name], metrics[name]
            if t['mode'] == 'periodic':
                if s['remaining'] is None and s['transfer_end'] is None:
                    s.update(remaining=t['cpu_us'], deadline=now + t['period_us'])
                else:
                    m['dropped_jobs'] += 1
                s['next_release'] += t['period_us']
            else:
                s.update(remaining=t['cpu_us'], next_release=end)
        while stall_index < len(stalls) and stalls[stall_index]['at_us'] + stalls[stall_index]['duration_us'] <= now:
            stall_index += 1
        locked = stall_index < len(stalls) and stalls[stall_index]['at_us'] <= now
        candidates = [name for name, s in states.items() if s['remaining'] is not None or
                      (s['transfer_end'] is not None and specs[name]['wait'] == 'busy')]
        selected = max(candidates, key=lambda n: specs[n]['priority']) if candidates and not locked else None
        future = [end]
        if event_index < len(events):
            future.append(events[event_index]['at_us'])
        while boundary_index < len(boundaries) and boundaries[boundary_index] <= now:
            boundary_index += 1
        if boundary_index < len(boundaries):
            future.append(boundaries[boundary_index])
        for s in states.values():
            future.append(s['next_release'])
            if s['transfer_end'] is not None:
                future.append(s['transfer_end'])
        if selected is not None and states[selected]['remaining'] is not None:
            future.append(now + states[selected]['remaining'])
        until = min(t for t in future if t > now)
        delta = until - now
        for name, s in states.items():
            if s['active'] and specs[name]['mode'] == 'buffered':
                m = metrics[name]
                m['underrun_us'] += max(0, delta - s['buffer'])
                s['buffer'] = max(0, s['buffer'] - delta)
                m['min_buffer_us'] = min(m['min_buffer_us'], s['buffer'])
        if locked or selected is not None:
            cpu_busy += delta
        if selected is not None and states[selected]['remaining'] is not None:
            states[selected]['remaining'] -= delta
        now = until
        for name, s in states.items():
            if s['remaining'] != 0:
                continue
            t, m = specs[name], metrics[name]
            s['remaining'] = None
            if t['mode'] == 'periodic':
                s['transfer_end'] = now + t['transfer_us']
                if s['transfer_end'] == now:
                    finish_periodic(name)
            else:
                m['completed'] += 1
                if not s['first_write']:
                    m['max_startup_us'] = max(m['max_startup_us'], now - s['activation'])
                    s['first_write'] = True
                if s['last_write'] is not None:
                    m['max_refill_interval_us'] = max(m['max_refill_interval_us'], now - s['last_write'])
                s['last_write'] = now
                s['buffer'] = min(t['capacity_us'], s['buffer'] + t['refill_us'])
                s['next_release'] = now + max(0, s['buffer'] - (t['capacity_us'] - t['refill_us']))
    for name, s in states.items():
        if s['transfer_end'] is not None and s['transfer_end'] <= end:
            finish_periodic(name)
        close_pending(name)
    failures = []
    for name, task in specs.items():
        for limit, allowed in task['limits'].items():
            metric = limit if limit in ('max_lateness_us', 'max_startup_us') else limit.removeprefix('max_')
            value = metrics[name][metric]
            if value > allowed:
                failures.append(f'{name}.{metric}: {value} > {allowed}')
    memory = profile['memory']
    remaining = memory['available_bytes'] - sum(a['bytes'] for a in memory['allocations'])
    if remaining < memory['reserve_bytes']:
        failures.append('allocation envelope consumes the reserved memory margin')
    for allocation in memory['allocations']:
        if allocation['bytes'] > memory['largest_block_bytes']:
            failures.append(f"allocation {allocation['name']} exceeds the observed largest block")
    calibrated = profile['calibration']['status'] == 'measured'
    if require_calibrated and not calibrated:
        failures.append('device calibration is incomplete; this is not a release gate PASS')
    return dict(model_version=MODEL_VERSION, result='FAIL' if failures else 'PASS',
                calibration=profile['calibration'], tasks=metrics,
                metrics=dict(cpu_busy_us=cpu_busy, cpu_busy_percent=round(100 * cpu_busy / end, 3),
                             memory_remaining_bytes=remaining), failures=failures,
                calibrated_resource_acceptance=('FAIL' if failures else 'PASS') if calibrated else 'NOT RUN',
                hardware_acceptance='NOT RUN')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', type=Path, required=True)
    parser.add_argument('--trace', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--require-calibrated', action='store_true')
    args = parser.parse_args(argv)
    try:
        profile_bytes, trace_bytes = args.profile.read_bytes(), args.trace.read_bytes()
        report = simulate(json.loads(profile_bytes), json.loads(trace_bytes), require_calibrated=args.require_calibrated)
        report.update(profile_sha256=hashlib.sha256(profile_bytes).hexdigest(),
                      trace_sha256=hashlib.sha256(trace_bytes).hexdigest(),
                      model_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
        output = json.dumps(report, indent=2) + '\n'
        if args.output:
            args.output.write_text(output, encoding='utf-8')
        print(output, end='')
        return 0 if report['result'] == 'PASS' else 1
    except (ValueError, KeyError, TypeError, OSError) as error:
        print(f'Invalid stress input: {error}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
