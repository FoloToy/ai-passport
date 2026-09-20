<p align="right">
  <a href="on-device-game-ai-wall-clock-budget.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Wall-clock Budgets for On-Device Game AI

Collected after releasing **Connect Four**, whose computer opponent searches a
10 × 7 board on an ESP32-C3 with no PSRAM.

## Do not bound a search by node count

The first implementation capped the search at a fixed number of nodes. Measured on
the device, 150,000 nodes took about 11 seconds — roughly 15,000 nodes per second,
an order of magnitude slower than a desktop estimate, because the core is a
single-issue RISC-V part running from flash cache. The move also starved the idle
task for long enough to trip the task watchdog.

The fix is to bound the search by **wall-clock time** and let the depth adapt:
iterative deepening from depth 2 upward, one complete iteration always kept as the
answer, and a budget of about half a second per move. On this board that lands
around 8,000 to 12,000 nodes and plays a solid casual game, and a slower or faster
board automatically searches shallower or deeper instead of hanging.

## Yield, and check the budget at a useful granularity

A CPU-bound worker above idle priority that runs for seconds will trip the task
watchdog, so the search calls back into the platform every N nodes. That callback
does two things: `vTaskDelay(1)` to let the idle task run, and compare the current
time against the deadline; returning false abandons the iteration and the previous
complete result is used.

Pick N from the platform's node rate, not from taste: 4,096 nodes was about 270 ms
of overshoot here, so the interval is 1,024 nodes (roughly 70 ms of overshoot, well
under 2% overhead from the yields).

## Keep the search platform-free, and make difficulty a product decision

The search itself has no ESP-IDF dependency. The time budget, the clock, and the
yield are all injected through an options struct, which keeps the module
host-testable: unit tests run deterministic searches with the budget disabled,
assert immediate wins and blocks, and play full self-play games.

Difficulty is expressed as a **blunder rate** rather than a shallower search. A
shallow search does not play a weaker game — it stops seeing threats and looks
broken. A small probability of playing a random legal move, evaluated before the
forced win/block checks, produces an opponent that is clearly beatable while still
punishing an obvious mistake.

Host tests cover the model, the geometry, the menu state machine, and the search;
the device gives the AI roughly half a second per move, which reads as thinking
rather than waiting.
