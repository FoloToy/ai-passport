<p align="right">
  <a href="three-key-reader-interaction.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Three-Key Reader Interaction on the AI Passport

Captured after the **Saya no Uta reader** release, from on-device testing with the
serial log open. General for any three-button app on this board.

> **Verification status.** Every behaviour below was observed on hardware with the
> released firmware: the button driver's event sequence was captured from the serial log,
> and the list, warning-gate and fast-forward behaviours were exercised on the device.

## The button driver merges quick taps - handle the double click

`iot_button` groups presses that arrive inside its short-press window. Two quick taps
produce `PRESS_UP`, `PRESS_UP`, then a single `BUTTON_DOUBLE_CLICK`; the single-click
callbacks are **not** emitted for either tap, so an app that only handles `SINGLE_CLICK`
silently drops input that feels perfectly normal to a reader (tap, tap, tap through
dialogue). Three or more taps in the window produce `BUTTON_MULTIPLE_CLICK`, which is
not registered by default either.

Practical rule: treat `BSP_BTN_DOUBLE` like a normal click, and if the app cares about
long runs of taps, register `MULTIPLE_CLICK` handlers with explicit click counts. The
serial log is the fastest way to see this: log the button and event name on every event
that is not a raw press, and the sequence reveals the merge immediately.

```text
I (24200) saya: <key> UP <event> release
I (24420) saya: <key> UP <event> release
I (24545) saya: <key> UP <event> double-click   <- two taps, one event
```

(The firmware logs each key event in the project's own language; the events above are
shown translated. Grep the log for the double-click event name and the merge is visible.)


## Hold to repeat needs a long-press threshold and the release event

Register `BUTTON_LONG_PRESS_START` *and* `BUTTON_PRESS_UP`; then a 1 s long press can start
fast-forwarding and the release stops it. Keeping the threshold at 300 ms made it almost
impossible to press a key once without starting a fast-forward, so the threshold is a UX
parameter, not just a driver detail (`BSP_BTN_LONG_MS = 1000` in this board's BSP).

## Do not wrap list cursors

A list that wraps makes "up" on the first row jump to the last row; users report that as
"the up and down keys are reversed", and it looks like a hardware or mapping bug even
though the mapping is fine. Clamp at both ends instead, and the same rule for two-option
choice lists: at the top row "up" stays put.

## Gate a first-boot disclaimer on scrolling to the end

For a content warning that must actually be read, scroll the text in a container with
`LV_LABEL_LONG_WRAP` and query `lv_obj_get_scroll_bottom()` (after
`lv_obj_update_layout()`) to know whether the end has been reached:

- while the text is unread, show a "scroll to the end" hint, and make OK page the text
  down instead of entering the app;
- once the end is reached, switch the hint to "press OK to continue" and accept the key.

Show scroll hints only when the content actually overflows, otherwise the screen claims
to scroll when there is nothing to see. The same helper works for an "about" screen.

## Keep screen geometry independent of content

Two layout rules that survived device review: give the text box a fixed height plus a
bottom margin instead of letting it run to the screen edge (the last line otherwise sits
on the bezel), and place the speaker name outside the box (above it, in the art area) so
a long name cannot push the body text around. Both are cheap to get right at build time
and annoying to discover on the device.
