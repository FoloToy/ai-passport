"""Manual and automatic Hardware Buddy heartbeat scenarios."""

from dataclasses import dataclass
from typing import Any, Dict, List, Optional


@dataclass(frozen=True)
class Scenario:
    name: str
    total: int
    running: int
    message: str


class ScenarioController:
    AUTO: List[Scenario] = [
        Scenario("Idle", 1, 0, "Claude is ready"),
        Scenario("Busy", 1, 1, "Working on your request"),
        Scenario("Approval", 1, 0, "approve: Bash"),
        Scenario("Completed", 1, 0, "Task completed"),
        Scenario("Sleep", 0, 0, "No Claude connected"),
    ]

    def __init__(self) -> None:
        self.total = 1
        self.running = 0
        self.message = "Claude is ready"
        self.entries: List[str] = []
        self.tokens = 0
        self.tokens_today = 0
        self.prompt: Optional[Dict[str, str]] = None
        self._auto_index = -1

    def set_idle(self) -> None:
        self.total, self.running, self.message = 1, 0, "Claude is ready"
        self.prompt = None

    def set_busy(self) -> None:
        self.total, self.running, self.message = 1, 1, "Working on your request"
        self.prompt = None

    def set_completed(self) -> None:
        self.total, self.running, self.message = 1, 0, "Task completed"
        self.prompt = None
        self.tokens += 1000
        self.tokens_today += 1000

    def set_sleep(self) -> None:
        self.total, self.running, self.message = 0, 0, "No Claude connected"
        self.prompt = None

    def set_approval(self, request_id: str, tool: str, hint: str) -> None:
        self.total, self.running, self.message = 1, 0, "approve: " + tool
        self.prompt = {"id": request_id, "tool": tool, "hint": hint}

    def clear_prompt(self) -> None:
        self.prompt = None
        self.message = "Claude is ready"

    def advance_auto(self) -> Scenario:
        self._auto_index = (self._auto_index + 1) % len(self.AUTO)
        scenario = self.AUTO[self._auto_index]
        if scenario.name == "Idle":
            self.set_idle()
        elif scenario.name == "Busy":
            self.set_busy()
        elif scenario.name == "Approval":
            self.set_approval("auto-approval", "Bash", "echo automatic scenario")
        elif scenario.name == "Completed":
            self.set_completed()
        else:
            self.set_sleep()
        return scenario

    def heartbeat(self) -> Dict[str, Any]:
        result: Dict[str, Any] = {
            "total": self.total,
            "running": self.running,
            "waiting": 1 if self.prompt else 0,
            "message": self.message,
            "entries": list(self.entries),
            "tokens": self.tokens,
            "tokens_today": self.tokens_today,
        }
        if self.prompt:
            result["prompt"] = dict(self.prompt)
        return result
