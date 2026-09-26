<p align="right">
  <a href="three-key-input-event-semantics.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Three-Key Input Semantics: Only Clicks and Long Presses Are User Intent

Collected after releasing **ATRI Reader**, whose three-key reader shipped a "hold DOWN
to toggle auto-read" mode that switched itself off immediately after being switched
on.

## The release event is not a key press

The button component reports PRESS, RELEASE, CLICK and LONG. The first version
cancelled the auto-read mode on *any* event, so the long press that enabled it was
followed by its own release event and the mode died instantly. The switch, the
handler and the long-press detection were all correct — the bug was which events
count as intent.

Rules worth keeping:

- Only **CLICK** and **LONG** mean "the user asked for this". Treat RELEASE as a
  signal that a key is no longer held, never as a command.
- Use RELEASE for exactly one purpose: ending a held-key behaviour (here,
  fast-forward). Stop that behaviour on a page change too, so a stale hold cannot
  keep advancing after the screen has moved on.
- A mode toggle triggered by a long press must ignore the release that follows it.

## Press timing is part of the contract

- **Short press threshold 120 ms**: contacts bounce, and a very short touch should
  not register as a command.
- **Long press 300 ms**: this reads as a deliberate hold while still feeling
  immediate. Ports that use one second for the same gesture make reading modes feel
  sluggish; shorten it if the long press is a primary action rather than a menu
  shortcut.
- Held-key repeat (fast-forward) belongs in the application tick, not in the button
  driver: the driver reports edges, the app decides the repeat rate and when to stop.

## Callback discipline

The button callback runs in the vendor component's shared `esp_timer` task. Enqueue
the event and return: do not take the LVGL lock, touch storage, or start audio from
there. The application's own input task drains the queue, handles one event per
iteration, and takes the LVGL lock once.

## Distinguish activity from waiting

The idle timers (dim, screen off, deep sleep) reset on real key events. A hands-off
auto-advance mode has to count as activity, or a long unattended run goes dark
mid-chapter; but "waiting for the player at a decision" must not, or the device
never sleeps on a screen that needs input. Express that as an explicit predicate
(mode enabled *and* actively advancing) instead of making the whole screen exempt.
