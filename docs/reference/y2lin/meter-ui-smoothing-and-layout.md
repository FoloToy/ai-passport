<p align="right">
  <a href="meter-ui-smoothing-and-layout.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Sound-Meter UI: Smoothing, Anchors, and Stray Blocks

Captured after the **Sound Sentry** meter page went through a full UI review
and a community release (commits `8abb233` and `517c7c8` through `dec556e`).
These lessons apply to any AI Passport app that shows a continuously-updating
measurement or a mascot-style character.

## Smooth a live readout with an asymmetric EMA

A raw pseudo-SPL value sampled every 80 ms makes the on-screen number, bar,
and zone color jump constantly and read as noise. Smooth them in the model
layer with an asymmetric EMA — fast attack (~130 ms) so real increases show
immediately, slow release (~0.5 s) so the readout settles instead of twitching
down — implemented in Q8 fixed-point with first-frame passthrough. Keep raw
values wherever accuracy matters: peak/mean statistics and the alarm state
machine must not lag behind reality. Preserve the smoother across session
resets, or the readout flashes to zero on every OK-key reset. Attack, release,
and convergence behavior lives in the host-testable model — test it there,
not on device.

## Give big numbers a fixed-width box

A 28 pt reading left to its natural width collides with the dB unit and the
status badge the moment the value crosses 100 dB and gains a third digit.
Right-align the number inside a fixed-width box and align scale ticks to the
panel edge, so digit-count changes never move neighboring elements.

## Anchor animations at creation, never at the current position

The mascot bobbed and jumped higher until it eventually left the screen: the
bob/jump animations used the *current* y as their start position, so
rebuilding the animation while the mascot was mid-swing (volume hovering on a
zone boundary) shifted the whole swing window upward on every rebuild. Start
such animations from a base y captured at creation time; the swing window then
stays constant no matter how often it is rebuilt.

## A stray block on screen has several usual suspects

The same user-visible symptom — an unexpected colored block — had four
different root causes in this one page. Enumerate the suspects instead of
guessing:

- **An orphaned shadow.** `ui_pixel_panel_create()` builds two sibling
  objects, the ink drop shadow and the paper body; hiding only the body left
  the shadow on screen forever. Popups must show and hide both as a pair —
  `ui_pixel_panel_create_ex()` returns the shadow for exactly this.
- **A dark slot at low fill.** A dark slate bar track (0x2E3A44) reads as a
  stray black block while the indicator is short; a light paper-gray track
  (0xE2E2D6) keeps the slot visually part of the page.
- **Pure decoration.** A bottom grass strip read as an unexpected block until
  removed; the same goes for any oversized decoration added for flavor.
- **An object covering another.** The mascot overlapped the stats panel and
  visually buried a statistic value underneath it.

## A white screen at boot can be LVGL pool exhaustion

The meter page (about 90 objects, roughly 35 KB LVGL allocation peak)
exhausted the template's 24 KB built-in LVGL pool mid-build:
`lv_obj_create()` returned NULL, the following style call dereferenced it,
and with the backlight already on no frame ever rendered — a white screen,
not a crash report. Switch LVGL to the C-library allocator
(`CONFIG_LV_USE_CLIB_MALLOC=y`) to fold the static pool into the general
heap, and raise the stack of the tasks that build pages and run key
callbacks — the 3584-byte default starved both `app_main` and the esp_timer
task; 8192 is comfortable. Log post-build free heap when entering a page so
field diagnosis is possible.

## Generalization for the next app

- Smooth what humans read; keep raw values for everything machines decide.
- Fixed-width containers for any numeric label that can change digit count.
- One creation-time anchor per animated object; never rebase on the current
  position.
- When a stray block appears, enumerate shadows, slots, decorations, and
  overlaps before touching code.
- On a white screen, suspect a NULL-object crash with the backlight on before
  suspecting the display driver; check the LVGL allocator budget.

## Related documents

- `docs/CHANGELOG.md` — the Unreleased entries for the meter UI and the
  sound-meter application.
- `main/demo_sound_meter.c` and `main/sound_meter_model.c` — the page and
  its host-testable model.
- `main/ui_pixel.c` — the panel/shadow pair helpers.
