#!/usr/bin/env python3
"""Positive and negative controls for the deterministic resource model."""
import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("game_resource_stress", ROOT / "tools/game_resource_stress.py")
model = importlib.util.module_from_spec(spec)
spec.loader.exec_module(model)
FIXTURES = ROOT / "tests/fixtures/game_resource_stress"


class StressTests(unittest.TestCase):
    def setUp(self):
        self.profile = json.loads((FIXTURES / "profile.json").read_text())
        self.trace = json.loads((FIXTURES / "trace.json").read_text())

    def run_model(self):
        return model.simulate(self.profile, self.trace)

    def test_blocking_wait_allows_lower_priority_audio(self):
        result = self.run_model()
        self.assertEqual(result["result"], "PASS", result["failures"])
        self.assertEqual(result["metrics"]["audio_underrun_us"], 0)
        self.assertGreater(result["metrics"]["audio_writes"], 500)
        self.assertGreater(result["metrics"]["frames_completed"], 50)
        self.assertEqual(result["hardware_acceptance"], "NOT RUN")

    def test_busy_wait_reproduces_audio_starvation(self):
        self.profile["cpu"]["display_wait"] = "busy"
        result = self.run_model()
        self.assertEqual(result["result"], "FAIL")
        self.assertGreater(result["metrics"]["audio_underrun_us"], 1_000_000)

    def test_priority_preemption_prevents_busy_wait_starvation(self):
        self.profile["cpu"].update(display_wait="busy", audio_priority=5)
        result = self.run_model()
        self.assertEqual(result["metrics"]["audio_underrun_us"], 0)
        self.assertEqual(result["result"], "PASS", result["failures"])

    def test_nonpreemptible_stall_overflows_buffer_even_at_high_priority(self):
        self.profile["cpu"]["audio_priority"] = 5
        self.profile["stalls"] = [{"at_us": 4_000_000, "duration_us": 120_000}]
        result = self.run_model()
        self.assertEqual(result["result"], "FAIL")
        self.assertGreater(result["metrics"]["audio_underrun_us"], 30_000)

    def test_slow_synthesis_and_frame_deadline_fail(self):
        self.profile["cpu"].update(audio_render_us=9000, audio_priority=5, frame_period_us=60000)
        result = self.run_model()
        self.assertGreater(result["metrics"]["audio_underrun_us"], 0)
        self.assertGreater(result["metrics"]["dropped_frames"], 0)
        self.assertEqual(result["result"], "FAIL")

    def test_dma_can_complete_while_cpu_is_stalled(self):
        self.trace = {"schema": 1, "duration_us": 130000, "events": [
            {"at_us": 0, "audio": False, "render": True}]}
        self.profile["stalls"] = [{"at_us": 55000, "duration_us": 70000}]
        result = self.run_model()
        self.assertEqual(result["metrics"]["frames_completed"], 1)
        self.assertEqual(result["metrics"]["max_frame_lateness_us"], 0)

    def test_title_cancels_pending_work_and_demand(self):
        self.trace = {"schema": 1, "duration_us": 150000, "events": [
            {"at_us": 0, "audio": True, "render": True},
            {"at_us": 10000, "audio": False, "render": False}]}
        self.profile["cpu"]["audio_priority"] = 5
        result = self.run_model()
        self.assertEqual(result["metrics"]["audio_writes"], 2)
        self.assertEqual(result["metrics"]["frames_completed"], 0)
        self.assertEqual(result["metrics"]["audio_underrun_us"], 0)

    def test_empty_title_has_no_jobs(self):
        self.trace["events"] = [self.trace["events"][0]]
        result = self.run_model()
        self.assertEqual(result["metrics"]["cpu_busy_us"], 0)
        self.assertEqual(result["metrics"]["audio_writes"], 0)

    def test_memory_total_and_contiguous_limit_are_separate(self):
        self.profile["memory"]["allocations"] = [{"name": "large image", "bytes": 105000}]
        result = self.run_model()
        self.assertEqual(result["result"], "FAIL")
        self.assertEqual(len(result["failures"]), 2)

    def test_buffer_boundary_and_first_write_delay(self):
        self.profile["cpu"]["audio_priority"] = 5
        self.profile["cpu"]["audio_render_us"] = 8000
        result = self.run_model()
        self.assertEqual(result["metrics"]["audio_underrun_us"], 0)
        self.assertEqual(result["metrics"]["max_startup_us"], 8000)
        self.profile["limits"]["max_startup_us"] = 7999
        self.assertEqual(self.run_model()["result"], "FAIL")

    def test_missing_first_write_is_not_hidden_by_prefill(self):
        self.profile["stalls"] = [{"at_us": 2000000, "duration_us": 8000000}]
        result = self.run_model()
        self.assertEqual(result["metrics"]["max_startup_us"], 8000000)
        self.assertEqual(result["metrics"]["audio_writes"], 0)

    def test_repeated_activation_and_determinism(self):
        self.trace["events"] += [{"at_us": 11000000, "audio": True, "render": True}]
        first = self.run_model()
        self.assertEqual(first, self.run_model())
        self.assertEqual(first["result"], "PASS", first["failures"])

    def test_reject_invalid_values_and_out_of_order_events(self):
        for key, value in [("frame_period_us", 0), ("render_us", True), ("audio_render_us", -1)]:
            with self.subTest(key=key):
                profile = copy.deepcopy(self.profile)
                profile["cpu"][key] = value
                with self.assertRaises(ValueError):
                    model.simulate(profile, self.trace)
        self.trace["events"][1]["at_us"] = 0
        with self.assertRaises(ValueError):
            self.run_model()

    def test_reject_equal_priority_and_unmeasured_calibration(self):
        self.profile["cpu"]["audio_priority"] = 4
        with self.assertRaises(ValueError):
            self.run_model()
        self.setUp()
        self.profile["calibration"]["status"] = "measured"
        with self.assertRaises(ValueError):
            self.run_model()

    def test_cli_output_hashes_and_calibration_gate(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "report.json"
            command = [sys.executable, str(ROOT / "tools/game_resource_stress.py"),
                       "--profile", str(FIXTURES / "profile.json"),
                       "--trace", str(FIXTURES / "trace.json"), "--output", str(output)]
            run = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stderr)
            report = json.loads(output.read_text())
            self.assertEqual(len(report["profile_sha256"]), 64)
            self.assertEqual(report["result"], "PASS")
            run = subprocess.run(command + ["--require-calibrated"], capture_output=True, text=True)
            self.assertEqual(run.returncode, 1)
            self.assertIn("calibration is incomplete", run.stdout)

    def test_cli_invalid_input_returns_two(self):
        with tempfile.TemporaryDirectory() as directory:
            profile = Path(directory) / "invalid.json"
            profile.write_text("{")
            run = subprocess.run([sys.executable, str(ROOT / "tools/game_resource_stress.py"),
                                  "--profile", str(profile), "--trace", str(FIXTURES / "trace.json")],
                                 capture_output=True, text=True)
            self.assertEqual(run.returncode, 2)


if __name__ == "__main__":
    unittest.main()
