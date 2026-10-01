<p align="right">
  <a href="adc-ladder-keyboard-key-lock.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# A Key Lock for the ADC-Ladder Keyboard

Captured after the **Pocket Intercom** three-key controls were validated on real
hardware (2026-10-01). These findings are general and upstream-benefiting: the
board reads all three keys from one ADC pin, so any application on this hardware
shares the same failure mode.

> **Verification status.** Reproduced and fixed on a real AI Passport board:
> the spurious event was observed in serial key logs before the fix and absent
> after it, with the thresholds below in place. Contact-level behaviour is
> electrical and part-dependent; the numbers here are one unit, not a
> characterisation of the whole production batch.

## The hardware shape

The three keys sit on a single ADC input as a resistor ladder: each key connects a
different tap, so the pin voltage identifies which key is down (or that none is).
The application gets `PRESS` / `HOLD` / `RELEASE` style events from the BSP after
the raw level is mapped to a key through voltage windows.

Two consequences follow immediately:

- Keys cannot be pressed simultaneously without producing a voltage that belongs to
  no window; the input is inherently one-key-at-a-time.
- Because the identification is a voltage comparison, **any** change in the
  measured level — not just a real press — can be read as another key.

## The failure mode: a long press that reports a different key

While a key is held, the contact is not perfectly stable: the measured level can
momentarily drift or open, and the reading can land inside the window of a
**neighbouring** key. The result is exactly the kind of bug that is hard to
believe at first: press and hold OK, and the device acts as if UP was pressed.

Notes from observing it:

- It correlates with press duration, not with contact cleanliness: very short
  presses are essentially never affected, long presses occasionally are. That is
  why it appears in features that use a long press (settings, fast-forward) while
  normal menu navigation looks fine.
- It is invisible in a single screenshot or a single log line taken at the wrong
  moment; it needs a key event log with timestamps to see the transition.
- Any code that treats "a new key-down while a key is already down" as a real
  user intent will act on the spurious key.

## The fix: lock the key while it is held

Two small changes, both in the BSP rather than in each application:

1. **Key lock.** Once a key-down has been accepted, all further key-down events
   are ignored until that key reports its release. The input is documented as
   one-key-at-a-time, so this matches the hardware instead of fighting it.
2. **Explicit timing thresholds.** Short-press and long-press thresholds are
   configured explicitly (180 ms and 500 ms here) instead of relying on defaults,
   so the classification of a press is the same in every application and can be
   reasoned about in tests.

With both in place, a long press behaves as a long press, and a neighbouring key
can no longer fire from contact noise.

## Diagnostics that made it findable

- Keep a serial "key black box": log each raw level change with a timestamp and
  the key it mapped to, plus the accepted event. The spurious transition is
  obvious in that log and almost invisible anywhere else.
- Log the transitions that the key lock **drops**. A dropped event that arrives
  immediately before a legitimate release is the signature of this failure mode,
  and it also tells you that the lock is doing its job.
- Do not "fix" it by widening the voltage windows: the windows are already sized
  for the ladder, and widening them moves the problem to the boundary between
  adjacent keys instead of removing it.

## What to copy

- Put the key lock and the press thresholds in the BSP so every application
  inherits them; per-application debouncing will drift and disagree.
- Treat the input as one-key-at-a-time in the API contract, and say so in the
  header, so application code does not invent two-key gestures that the hardware
  cannot report.
- Ship a key event log you can turn on; it is the difference between "the button
  is flaky" and a one-line fix.

The explicit press and hold thresholds were also contributed upstream as a BSP
change so the same timing is available without each application re-deriving it.
