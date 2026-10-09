#!/usr/bin/env python3
"""General model acceptance, including an independent 1-us scheduling oracle."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import random
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('resource_stress', ROOT / 'tools/resource_stress.py')
model = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(model)
FIXTURES = ROOT / 'tests/fixtures/resource_stress'


def fixture(name):
    return tuple(json.loads((FIXTURES / f'{name}-{kind}.json').read_text())
                 for kind in ('profile', 'trace'))


def periodic(name='worker', priority=1, cpu=2, period=10, transfer=0, wait='blocking'):
    return dict(id=name, mode='periodic', priority=priority, cpu_us=cpu,
                period_us=period, transfer_us=transfer, wait=wait,
                limits=dict(max_lateness_us=0, max_dropped_jobs=0))


def buffered(name='stream', priority=2, cpu=1, refill=4, capacity=12, initial=12):
    return dict(id=name, mode='buffered', priority=priority, cpu_us=cpu,
                refill_us=refill, capacity_us=capacity, initial_buffer_us=initial,
                limits=dict(max_underrun_us=0, max_startup_us=3))


def example(tasks, duration=100):
    return (dict(schema=2, tasks=tasks, memory=dict(available_bytes=1000,
        largest_block_bytes=500, reserve_bytes=200, allocations=[]),
        calibration=dict(status='synthetic', source='Analytical example', assumptions=['Illustrative costs'])),
        dict(schema=2, duration_us=duration, events=[dict(at_us=0, active=[t['id'] for t in tasks])]))


def tick_oracle(profile, trace):
    """Small-input reference: enumerate every microsecond, never skip to events."""
    tasks = profile['tasks']
    states = {t['id']: dict(active=False, cpu=0, transfer=None, next=None,
                            buffer=0, completed=0, dropped=0, underrun=0) for t in tasks}
    events = {e['at_us']: set(e['active']) for e in trace['events']}
    busy = 0
    for now in range(trace['duration_us'] + 1):
        for t in tasks:
            s = states[t['id']]
            if s['transfer'] == now:
                s['completed'] += 1
                s['transfer'] = None
        if now == trace['duration_us']:
            break
        if now in events:
            for t in tasks:
                s = states[t['id']]
                active = t['id'] in events[now]
                if active != s['active']:
                    s.update(active=active, cpu=0, transfer=None,
                             next=now if active else None,
                             buffer=t.get('initial_buffer_us', 0) if active else 0)
        for t in tasks:
            s = states[t['id']]
            if s['next'] == now:
                if t['mode'] == 'periodic':
                    s['next'] = now + t['period_us']
                    if s['cpu'] or s['transfer'] is not None:
                        s['dropped'] += 1
                    else:
                        s['cpu'] = t['cpu_us']
                else:
                    s['cpu'], s['next'] = t['cpu_us'], None
        locked = any(x['at_us'] <= now < x['at_us'] + x['duration_us'] for x in profile.get('stalls', []))
        ready = [t for t in tasks if states[t['id']]['cpu'] or
                 (states[t['id']]['transfer'] is not None and t.get('wait') == 'busy')]
        chosen = max(ready, key=lambda t: t['priority']) if ready and not locked else None
        busy += int(locked or chosen is not None)
        for t in tasks:
            s = states[t['id']]
            if s['active'] and t['mode'] == 'buffered':
                if s['buffer']:
                    s['buffer'] -= 1
                else:
                    s['underrun'] += 1
        if chosen:
            s = states[chosen['id']]
            if s['cpu']:
                s['cpu'] -= 1
                if s['cpu'] == 0:
                    if chosen['mode'] == 'periodic':
                        s['transfer'] = now + 1 + chosen['transfer_us']
                    else:
                        s['completed'] += 1
                        s['buffer'] = min(chosen['capacity_us'], s['buffer'] + chosen['refill_us'])
                        s['next'] = now + 1 + max(0, s['buffer'] - chosen['capacity_us'] + chosen['refill_us'])
    return states, busy


class ResourceStressTests(unittest.TestCase):
    def setUp(self):
        self.p, self.t = fixture('reader')

    def run_model(self):
        return model.simulate(self.p, self.t)

    def test_two_non_game_consumers(self):
        for name in ('reader', 'recorder'):
            with self.subTest(name=name):
                p, t = fixture(name)
                report = model.simulate(p, t)
                self.assertEqual(report['result'], 'PASS', report['failures'])
                self.assertTrue(all(x['completed'] > 0 for x in report['tasks'].values()))
                self.assertEqual(report['hardware_acceptance'], 'NOT RUN')
        # Independently derived exact job counts for three periodic consumers.
        self.assertEqual({k: v['completed'] for k, v in report['tasks'].items()},
                         {'sample': 100, 'storage': 10, 'status': 5})

    def test_blocking_busy_and_preemption(self):
        self.assertEqual(self.run_model()['result'], 'PASS')
        self.p['tasks'][0]['wait'] = 'busy'
        self.assertGreater(self.run_model()['tasks']['playback']['underrun_us'], 0)
        self.p['tasks'][1]['priority'] = 5
        self.assertEqual(self.run_model()['result'], 'PASS')

    def test_cpu_stall_drains_buffer(self):
        self.p['stalls'] = [dict(at_us=4000000, duration_us=120000)]
        report = self.run_model()
        self.assertEqual(report['result'], 'FAIL')
        self.assertGreaterEqual(report['tasks']['playback']['underrun_us'], 30000)

    def test_transfer_completes_during_cpu_stall(self):
        self.p, self.t = example([periodic(cpu=2, period=20, transfer=10)], 18)
        self.p['stalls'] = [dict(at_us=3, duration_us=15)]
        report = self.run_model()
        self.assertEqual(report['tasks']['worker']['completed'], 1)
        self.assertEqual(report['tasks']['worker']['max_lateness_us'], 0)

    def test_periodic_only_analytic_schedule(self):
        self.p, self.t = example([periodic(cpu=3, period=10, transfer=4)], 30)
        r = self.run_model()
        self.assertEqual(r['tasks']['worker']['completed'], 3)
        self.assertEqual(r['metrics']['cpu_busy_us'], 9)
        self.p['tasks'][0]['wait'] = 'busy'
        self.assertEqual(self.run_model()['metrics']['cpu_busy_us'], 21)

    def test_deadline_drops_and_unfinished_horizon(self):
        self.p, self.t = example([periodic(cpu=25)], 30)
        m = self.run_model()['tasks']['worker']
        self.assertEqual((m['completed'], m['dropped_jobs'], m['max_lateness_us']), (1, 2, 15))
        self.p['tasks'][0]['cpu_us'] = 100
        m = self.run_model()['tasks']['worker']
        self.assertEqual((m['completed'], m['max_lateness_us']), (0, 20))

    def test_cancel_overdue_job_records_delay(self):
        self.p, self.t = example([periodic(cpu=25)], 30)
        self.t['events'].append(dict(at_us=22, active=[]))
        m = self.run_model()['tasks']['worker']
        self.assertEqual((m['cancelled_jobs'], m['max_lateness_us'], m['completed']), (1, 12, 0))

    def test_buffered_only_empty_and_prefilled(self):
        self.p, self.t = example([buffered(cpu=2, refill=4, capacity=8, initial=0)], 10)
        m = self.run_model()['tasks']['stream']
        self.assertEqual((m['underrun_us'], m['max_startup_us'], m['completed']), (2, 2, 3))  # writes at 2, 4 and 8 us; next job releases at 10
        self.p['tasks'][0]['initial_buffer_us'] = 8
        self.assertEqual(self.run_model()['tasks']['stream']['underrun_us'], 0)

    def test_two_independent_buffer_consumers(self):
        self.p, self.t = example([buffered('network', 2), buffered('playback', 1)])
        r = self.run_model()
        self.assertEqual(r['result'], 'PASS', r['failures'])
        self.assertEqual(r['tasks']['network']['max_startup_us'], 1)
        self.assertEqual(r['tasks']['playback']['max_startup_us'], 2)
        self.assertTrue(all(x['completed'] > 20 for x in r['tasks'].values()))

    def test_no_first_write_cannot_be_hidden(self):
        self.p, self.t = example([buffered()], 10)
        self.p['stalls'] = [dict(at_us=0, duration_us=10)]
        m = self.run_model()['tasks']['stream']
        self.assertEqual((m['completed'], m['max_startup_us']), (0, 10))
        self.assertEqual(self.run_model()['result'], 'FAIL')

    def test_activation_idle_reentry_and_no_redundant_reset(self):
        self.p, self.t = example([buffered()], 20)
        self.t['events'] = [dict(at_us=0, active=[]), dict(at_us=5, active=['stream']),
                            dict(at_us=6, active=['stream']), dict(at_us=7, active=[]),
                            dict(at_us=15, active=['stream'])]
        m = self.run_model()['tasks']['stream']
        self.assertEqual(m['completed'], 2)
        self.assertEqual(m['max_startup_us'], 1)
        self.t['events'] = [dict(at_us=0, active=[])]
        self.assertEqual(self.run_model()['metrics']['cpu_busy_us'], 0)

    def test_memory_total_and_contiguous_fail_independently(self):
        self.p, self.t = example([periodic()])
        for size, failures in ((600, 1), (900, 2)):
            self.p['memory']['allocations'] = [dict(name='extra', bytes=size)]
            r = self.run_model()
            self.assertEqual(len(r['failures']), failures)
            self.assertEqual(r['metrics']['memory_remaining_bytes'], 1000-size)
        # No hardcoded ESP32 or 8 MiB limit in the reusable engine.
        self.p['memory'] = dict(available_bytes=2**32, largest_block_bytes=2**31,
                                reserve_bytes=2**30, allocations=[])
        self.assertEqual(self.run_model()['result'], 'PASS')

    def test_calibration_gate(self):
        r = model.simulate(self.p, self.t, require_calibrated=True)
        self.assertEqual(r['result'], 'FAIL')
        self.assertEqual(r['calibrated_resource_acceptance'], 'NOT RUN')
        self.p['calibration']['status'] = 'measured'
        with self.assertRaises(ValueError):
            self.run_model()

    def test_invalid_inputs(self):
        mutations = [lambda p,t: p.update(schema=1), lambda p,t: p.update(schema=True),
            lambda p,t: p.update(tasks=[]),
            lambda p,t: p['tasks'][0].update(cpu_us=True),
            lambda p,t: p['tasks'][0].update(period_us=0),
            lambda p,t: p['tasks'][0].update(wait='sleepish'),
            lambda p,t: p['tasks'][1].update(priority=4),
            lambda p,t: p['tasks'][1].update(id='display'),
            lambda p,t: p['tasks'][1].update(initial_buffer_us=9999999),
            lambda p,t: t['events'][0].update(active=['missing']),
            lambda p,t: t['events'][0].update(active=['display','display']),
            lambda p,t: t['events'][1].update(at_us=0),
            lambda p,t: p.update(stalls=[dict(at_us=1,duration_us=10),dict(at_us=2,duration_us=1)]),
            lambda p,t: p['tasks'][0].update(period_us=1)]
        for mutation in mutations:
            p,t=copy.deepcopy(self.p),copy.deepcopy(self.t)
            mutation(p,t)
            with self.subTest(mutation=mutations.index(mutation)), self.assertRaises(ValueError):
                model.simulate(p,t)

    def test_seeded_workloads_match_microsecond_oracle(self):
        rng = random.Random(802)
        for case in range(150):
            tasks = [periodic('compute',3,rng.randint(1,8),rng.randint(8,24),rng.randint(0,15),rng.choice(['busy','blocking'])),
                     periodic('storage',1,rng.randint(1,8),rng.randint(9,30),rng.randint(0,12)),
                     buffered('stream',2,rng.randint(1,5),4,12,rng.choice([0,12]))]
            p,t = example(tasks, 160)
            p['stalls'] = [dict(at_us=40,duration_us=rng.randint(1,20))]
            t['events'] += [dict(at_us=70,active=['stream','storage']),dict(at_us=100,active=[]),
                            dict(at_us=110,active=[x['id'] for x in tasks])]
            actual=model.simulate(p,t)
            expected,busy=tick_oracle(p,t)
            with self.subTest(case=case):
                self.assertEqual(actual['metrics']['cpu_busy_us'],busy)
                for name,m in actual['tasks'].items():
                    self.assertEqual((m['completed'],m['dropped_jobs'],m['underrun_us']),
                        (expected[name]['completed'],expected[name]['dropped'],expected[name]['underrun']))
                self.assertEqual(actual,model.simulate(p,t))

    def test_cli_hashes_both_consumers_and_calibration_refusal(self):
        with tempfile.TemporaryDirectory() as directory:
            output=Path(directory)/'report.json'
            for name in ('reader','recorder'):
                command=[sys.executable,str(ROOT/'tools/resource_stress.py'),'--profile',
                    str(FIXTURES/f'{name}-profile.json'),'--trace',str(FIXTURES/f'{name}-trace.json'),'--output',str(output)]
                run=subprocess.run(command,capture_output=True,text=True)
                self.assertEqual(run.returncode,0,run.stderr)
                r=json.loads(output.read_text())
                self.assertEqual(r['model_sha256'],hashlib.sha256((ROOT/'tools/resource_stress.py').read_bytes()).hexdigest())
                self.assertEqual(r['profile_sha256'],hashlib.sha256((FIXTURES/f'{name}-profile.json').read_bytes()).hexdigest())
                run=subprocess.run(command+['--require-calibrated'],capture_output=True,text=True)
                self.assertEqual(run.returncode,1)
                self.assertIn('calibration is incomplete',run.stdout)
            invalid=Path(directory)/'invalid.json'
            invalid.write_text('{')
            command[command.index('--profile')+1]=str(invalid)
            self.assertEqual(subprocess.run(command,capture_output=True).returncode,2)


if __name__ == '__main__':
    unittest.main()
