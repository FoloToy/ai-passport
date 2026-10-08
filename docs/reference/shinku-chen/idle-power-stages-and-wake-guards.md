<p align="right">
  <a href="idle-power-stages-and-wake-guards.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Idle Power Stages and Wake Guards

Collected while giving a Wi-Fi voice assistant on this board a staged idle policy:
dim at 60 s, screen and codec off at 6 min, deep sleep as a fallback at 36 min. The
policy itself is ordinary; three of the things it ran into are worth writing down.

## A key held through the boot is graded a long press

The onboard function-button wake input is the shared ADC key node (GPIO0), which
is not the board's only deep-sleep wake source: the baseline also wakes from the
RTC timer.
The key pressed to wake the device stays held while the firmware comes up, so the
button component - which starts a few hundred milliseconds in - registers a press
that is already down and grades it a long press once it has been down long enough.
Here the BSP sets that threshold to 500 ms, so an unintended action fires almost
immediately: in our build DOWN mutes the device.

A fixed "ignore keys for N seconds" window does not fix this. Measured on hardware,
a 2-second window armed when the buttons are created expired at about 2350 ms, while
the long press was dispatched at 2499 ms. The component's callback itself fires
earlier, but engine rules (and good practice) let those callbacks only enqueue work,
and the application task that runs it is busy bringing the network up first.

What works is to follow the release instead of the clock: after a boot that reports
a deep-sleep GPIO wake, drop key **actions** until that press comes up
(`OnPressUp`). The key is not released until the user lets go, so the guard lasts
exactly as long as it has to, and a deadline (10 s here) is kept only as the safety
net for a stuck key. Two things to keep in mind:

- Guard actions, never the wake path. Waking must stay live in every state.
- Ask `esp_sleep_get_wakeup_causes()` for the bitmap and test bits. A GPIO wake and
  a leftover timer cause can both be reported, so comparing against a single cause
  is not enough.

This applies to any product on this board that wakes from deep sleep by key. Whether
a product arms key wake at all is a design choice, not a universal requirement: the
baseline wakes from the RTC timer, and only applications that need user-initiated
wake-up should add the key as an extra wake source.

## Automatic light sleep stretches the timer you schedule the sleep with

An idle policy normally counts its seconds with an `esp_timer` created with
`skip_unhandled_events = true`. That flag exists to keep the timer out of
light-sleep wakeup sources, so such a timer does not wake the chip out of light
sleep. The countdown therefore stops tracking wall-clock time the moment automatic
light sleep is enabled: a deadline of "60 s" can stretch to whatever the chip's
real wake-ups add up to. Frequency scaling has no such effect - the tick keeps
running.

If light sleep is wanted, move the deadlines to a real time base
(`esp_timer_get_time()` deltas) or accept measuring them in wake-ups. Two other
things have to be arranged first for light sleep to pay off on this board: the LVGL
task wakes at the refresh period, and the button ADC scan runs off its own timer, so
both keep the chip from staying asleep.

## Stopping the radio is not free

In a Wi-Fi product stopping the station is the largest single idle saving, and it
carries the most side effects. With the radio down, a protocol that holds a
connection keeps retrying, and the failures reach the user: in our build the MQTT
client retried on its own timer (about every 22 s) plus a 60-second retry at the
protocol level. One of those attempts raised a user-visible alert, which also played
a notification sound and reopened the codec on a device that was supposed to be
asleep - and the alert latched, so it was still on screen after the next key press.
Nothing in the board layer can stop that; the protocol has to be told the link is
gone on purpose.

When the radio-off window is bounded by a deep-sleep fallback, keeping the link is
usually the better trade. Measured on this board, half an hour with the radio up
costs roughly 10 mAh of the 500 mAh cell (an estimate from the parts list, about
2 %), and buys an instant wake with no reconnect, server push still arriving, and no
retry storm to quiet. The arithmetic changes with the window: over eight hours the
radio is worth about a third of the cell, and a long window has to stop the station
*and* suspend whatever keeps retrying while it is down.
