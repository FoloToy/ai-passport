<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Pocket Finder — a BLE proximity finder play for AI Passport

This branch (`feature/pocket-finder`) turns a FoloToy AI Passport into a **Bluetooth
proximity detector**. It scans for nearby BLE advertisers and shows how strong each
one is; you pick one, walk toward it, and move on to the next when you get there.

The firmware boots straight into the radar — this play is the product screen, so there
is no menu to walk through first. The baseline test menu is still reachable behind the
exit gesture (hold DOWN twice), so the hardware pages stay available.

It is a **play**, not an asset tracker. Read the honesty rule below before expecting
it to find a specific object.

## What it does not do

- **It does not identify your devices.** There is no MAC or owner matching. AirTag and
  Tile deliberately rotate their Bluetooth address and advertised identity for
  anti-tracking reasons, so a third-party scanner cannot reliably follow one. What you
  get is "a device is here and it is getting stronger", never "this is your keyring".
- **It does not tell you a direction.** The ESP32-C3 has one antenna and no
  angle-of-arrival capability, so bearing is physically unobtainable. Voice prompts and
  UI wording avoid any directional claim.
- **It does not report distance.** Only relative signal strength. Absolute distance
  claims ("within 1 metre") are deliberately absent.
- **It does not use NFC.** The board carries a passive NTAG213 tag with no MCU-facing
  interface; it can be read by a phone and cannot read anything itself.

## Pages and controls

Three buttons: UP, DOWN, OK (a single ADC resistor ladder on GPIO0).

**Intro** — shown once per firmware version. Explains that the dot angles are random
and carry no meaning. Only OK dismisses it.

**RADAR** — a ring with up to six dots. Each dot's **radius** encodes signal strength
(closer to the centre is stronger). Each dot's **angle is fixed for the session and
carries no information**; it exists only so you can keep track of the same dot. The
selected device's name and level are shown below the ring, and a permanent caption
reads `ANGLE IS RANDOM / CLOSER/FARTHER IS REAL`.

| Button | Action |
| --- | --- |
| UP / DOWN click | move the selection |
| OK click | focus the selected device |
| OK long | mute / unmute |
| DOWN long twice | leave the play (the first long press arms a confirmation) |

**FOCUS** — one device. A five-bar signal meter is the primary element, with a trend
word (`STRONGER` / `WEAKER` / `STEADY`) and the device name below it.

| Button | Action |
| --- | --- |
| OK click | jump to the next device |
| OK long | mute / unmute |
| UP long | show the smoothed dBm reading (diagnostic) |
| DOWN long twice | back to RADAR |

A Geiger-counter style beep tracks the signal level: one beep every 2500 ms at the
weakest level, tightening to every 80 ms at the strongest. Muting keeps the visuals and
stops the audio.

## Honest signalling

Because there is no identity matching, the UI never claims to have found *your* thing.
It reports what is measurable: `NO SIGNAL`, `WEAK - TRY ANOTHER DIRECTION`,
`GETTING STRONGER`, `VERY CLOSE - CHECK HERE`. "Try another direction" refers to
where **you** stand, not to where the device is.

## Layout and source map

```text
main/finder_rssi.c     median-of-3 + time-weighted EMA, levels, trend, timeout decay   [host-tested]
main/finder_table.c    device table, top-6 eviction, aging, fixed angles, next-device  [host-tested]
main/finder_beep.c     streaming PCM cadence generator with fades                      [host-tested]
main/finder_ui.c       page/button state machine, two-step exit, notices               [host-tested]
main/finder_scan.c     NimBLE observer (passive scan, duplicate reports kept)
main/finder_audio.c    beep output worker (priority 5, above the LVGL port)
main/finder_view.c     the three screens, drawn with plain LVGL widgets
main/demo_finder.c     wiring: scan callback and key callback only enqueue
tests/test_finder_*.c  host tests for the four pure-logic modules
```

The UI is built from scratch with ordinary LVGL widgets. It does not use the baseline
demo's `ui_pixel` shell.

## Build

Follow the repository's own environment guide. In short, with ESP-IDF 5.5.3 active:

```bash
./tools/validate.sh          # host tests + firmware gate
```

The validated merged image is `build/FoloToy-AI-Passport-full.bin`, written from offset
`0x0`. The application-only `build/FoloToy-AI-Passport.bin` belongs at `0x10000` and
must not be written to `0x0`.

## One build-configuration change

`CONFIG_BT_NIMBLE_ROLE_OBSERVER=y` is required to scan at all; the upstream defaults
ship it as `n`. It costs roughly 4-8 KB of flash. Setting it back to `n` restores the
advertising-only baseline, after which this play cannot scan and the radar page reports
`SCAN FAILED`.

The play never advertises, so the usual "continuous scanning starves the device's own
advertising" concern does not apply here. The scan duty cycle (40 ms window in a 100 ms
interval) is limited for power only.

## Wording deviations from the design document

Two deliberate departures, both driven by what the display can actually render:

- Trend is shown as the words `STRONGER` / `WEAKER` / `STEADY` rather than arrow glyphs.
  Only Montserrat 14 and 20 are enabled, and their glyph coverage for arrows is not
  guaranteed; a missing glyph renders as a blank box.
- The permanent caption is `ANGLE IS RANDOM` / `CLOSER/FARTHER IS REAL`. The full
  sentence is about 243 px wide at 14 px and would be clipped by the 240 px panel.

## Status

Build and host tests pass through the repository gate. On-device behaviour — scan
versus advertising coexistence, battery life, beep continuity, and whether the ring
misleads anyone into walking in a direction — has **not** been verified and is listed
as unverified device checks in the accompanying design document.

## License

This branch is derived from [`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport),
which is MIT licensed. The same license applies.
