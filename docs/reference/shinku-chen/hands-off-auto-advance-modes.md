<p align="right">
  <a href="hands-off-auto-advance-modes.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Hands-Off Auto-Advance: Pace from the End of the Text, and Count It as Activity

Collected after releasing **ATRI Reader**, where holding the down key starts a mode
that advances the story without any further input.

## Pace from the end of the typewriter, not from the start of the line

The first version advanced as soon as the text had finished appearing, with a 250 ms
floor to keep "instant" text speed from running away. On the device it read as a
blur: by the time the eye reached the end of a two-line sentence, the line was gone.

The version that shipped waits a fixed **700 ms after the typewriter finishes** and
then advances. Two details make that work:

- While the typewriter is still running, keep the countdown pinned at its full value
  instead of letting it drain, so the pause after a line does not depend on how long
  the line took to appear.
- Keep the dwell independent of the text-speed setting: the speed setting controls how
  a line appears, the dwell controls how long it stays readable.

## Stop conditions and continuity

- Stop at decisions and endings — those need the player.
- Keep running across scene and chapter transitions: a fade between chapters is not a
  stop, and cancelling the mode there forces the user to re-enable it every chapter.
- Any real key press hands control back immediately, and the mode toggle must not be
  cancelled by its own release event (see the three-key input entry).
- Do not persist the mode across a reboot: a device that comes up advancing by itself
  is confusing.
- Show the mode on screen. A small marker in the corner is enough, and it makes a
  behaviour change that happens without input discoverable.

## Interaction with the idle timers

Auto-advance is activity, so it must not feed the dim / screen-off / deep-sleep
timers: with the default 45 s dim and 150 s blank, a long unattended run would go
dark in the middle of a chapter. But waiting for the player at a decision is idle, so
the exemption is conditioned on the mode being enabled *and* actually advancing.

Measured behaviour after that change: an unattended auto-read stayed lit for as long
as it ran, and once the mode was switched off the idle timers started over from zero,
so the device still dimmed, blanked and slept normally on any screen that was waiting
for input.
