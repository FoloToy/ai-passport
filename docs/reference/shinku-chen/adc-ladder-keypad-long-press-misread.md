<p align="right">
  <a href="adc-ladder-keypad-long-press-misread.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# An ADC Ladder Keypad Can Read a Long Press as Another Key

Captured while bringing up the **Senren \* Banka** reader (2026-09-27). The three
keys of the AI Passport are not three GPIOs: they share one ADC pin and are told
apart by voltage windows, which makes a held key able to look like a different
key. These findings apply to any application on this board, because the defect is
in the electrical layer rather than in one application.

> **Verification status.** Measured on one AI Passport board (ESP32-C3 revision
> v1.1, 8 MB flash, no PSRAM) with a key-event black box recording the ADC
> millivolts each event was detected at. The trap, the lock and the fixed
> behaviour were exercised; the lock and the black box are fork-side additions
> (see below) and are **not** present in upstream `main`. The window values of
> other boards, wear over time and behaviour at low battery are **not** covered.

## What the three keys actually are

`components/bsp/include/bsp_pins.h` gives the ladder: all three keys pull one
shared pin (`ADC1_CH0`, GPIO0) to different voltages, and the BSP decides which
key is down by which window the reading falls into.

| Key | Window | Nominal (documented) | Measured here |
| --- | --- | --- | --- |
| UP | 0–150 mV | lowest step | — |
| DOWN | 150–447 mV | middle step | 303–309 mV |
| OK | 447–1900 mV | ≈ 595 mV | 601 mV |
| released | above the windows | ≈ 3300 mV | **≈ 2959 mV** |

Two things are worth carrying over from that table. First, the windows are not
equal: the OK window is about 1.45 V wide while UP's is 0.15 V, because it is the
one that has to keep clear of the released rail. Second, the released rail on a
real board is about 340 mV below the nominal figure, which is exactly why the
documented procedure is to measure each key on the board being tuned rather than
to copy numbers.

## Why a long press can read as another key

When a key is held, the contact is not perfect. If it opens for a few
milliseconds, the node voltage starts travelling toward the released rail — and
on its way it passes **through every window above it**. The button component is
polling per window, so it sees the other key as pressed and released:

1. A contact break inside a held UP (~0–150 mV) lets the voltage sweep upward.
2. It crosses DOWN (150–447 mV) and then OK (447–1900 mV).
3. A sweep that dwells in the OK window for anything like a debounce period is
   reported as **OK pressed, then released** — a click.
4. The application opens its menu while the user is still holding UP.

That is the whole symptom: short presses behave perfectly, and only long presses
misbehave. In this project the menu is opened by an OK click *or* long press, so
holding UP to fast-forward occasionally popped the menu open. The same shape
would hit any application where a long press has a side effect.

## The lock

The fork adds a key-identity lock on top of the upstream window-by-window
polling. It is **not** part of upstream `main`: the implementation lives in the
fork commit
[`e0c260d`](https://github.com/Shinku-Chen/ai-passport/blob/e0c260d1782c52f6edf513ce7fd0b3cae3564cf6/components/bsp/src/bsp_button.c)
(`feature/senren-banka`, also carried by `feature/dracu-riot`), in
`components/bsp/src/bsp_button.c`:

- **Confirm before accepting a press (40 ms).** A window must be stable for
  40 ms before the driver reports anything. This also skips the intermediate
  steps the voltage sweeps through while a key is being pressed down.
- **Ignore another window while locked (120 ms).** Once a key is locked, a
  reading in a different window is ignored; only 120 ms of continuous reading in
  that other window is accepted as a real key change.
- **Hold the release too (120 ms).** The voltage returning to the released region
  only counts as a release after it stays there for 120 ms, so a brief contact
  loss neither fires the other key nor ends the long press being held.

All three are short enough to be invisible in normal use — the published press
timing stays at 180 ms for a click and 500 ms for a long press
(`BSP_BTN_SHORT_PRESS_MS` / `BSP_BTN_LONG_PRESS_MS`) — and together they turn the
unstable window into a stable key.

## Proving it with a key-event black box

The fix was only worth trusting once the failure had been seen in numbers, so the
application keeps a small ring of the last 32 key events: elapsed milliseconds,
key, event, and **the ADC millivolts of the very sample the driver used to decide**
(`bsp_button_last_mv()`, a fork-side accessor added by the same commit, which
reads the same shared 1 ms sample the three drivers poll). The ring is read back
over the serial channel, so the machine can be used normally and examined
afterwards instead of being watched live.

What it showed, in order:

- Before the lock: sequences where a held key produced an extra event for a
  different key, with a millivolt reading inside that other key's window.
- After the lock: clean `press → long → release` runs only, with the millivolts
  where they belong (OK at 601 mV, DOWN at 303–309 mV), and no cross-key events
  while fast-forwarding.

## Related changes and what to do next time

- Pin the press timing explicitly in the BSP instead of inheriting the component's
  Kconfig defaults, so the feel and the electrical facts are tuned in the same
  file (the fork's `fix/bsp-button-press-500ms` change, upstream pull request
  #68).
- Log every key event together with its detected millivolts while bringing up an
  application. It costs one line and it is the difference between "the menu opens
  by itself sometimes" and a diagnosis.
- Treat a key that reports a different key mid-press as an electrical question
  first, not as an application bug: check the windows against measurements on
  that board before changing any application logic.
