import unittest

from windows_buddy_controller.scenarios import ScenarioController


class ScenarioTests(unittest.TestCase):
    def test_manual_approval_and_clear_prompt(self):
        controller = ScenarioController()
        controller.set_approval("req_1", "Bash", "git push")
        heartbeat = controller.heartbeat()
        self.assertEqual(heartbeat["waiting"], 1)
        self.assertEqual(heartbeat["prompt"]["id"], "req_1")

        controller.clear_prompt()
        heartbeat = controller.heartbeat()
        self.assertEqual(heartbeat["waiting"], 0)
        self.assertNotIn("prompt", heartbeat)

    def test_auto_cycle_has_expected_sequence(self):
        controller = ScenarioController()
        names = [controller.advance_auto().name for _ in range(5)]
        self.assertEqual(names, ["Idle", "Busy", "Approval", "Completed", "Sleep"])
        self.assertEqual(controller.heartbeat()["total"], 0)


if __name__ == "__main__":
    unittest.main()
