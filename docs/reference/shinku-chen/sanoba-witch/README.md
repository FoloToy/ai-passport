<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Sanoba Witch

A portrait visual novel reader ported from the Mi Band fan port of *Sanoba Witch*:
**101 chapters, 53,190 lines of dialogue, about 1.1 million
characters, five heroine routes and five endings**, read fully offline.

## Publish information

- **Title**: Sanoba Witch
- **Description**: Turn an AI Passport into a pocket visual novel. It opens on a
  title screen and you read from the first line to one of five endings; the whole
  script lives on the device, with no network and nothing to copy over. UP / DOWN
  advance a line, UP (hold) fast-forwards, DOWN (hold) toggles auto-read, and OK
  opens the menu (save, load, jump to the next choice, auto-play). Every scene
  change is saved automatically, and "continue" on the title screen resumes it.
- **Repository**: <https://github.com/Shinku-Chen/ai-passport/tree/feature/sanoba-witch>
- **Release**: `v0.1.0-sanoba-witch` — merged image `FoloToy-AI-Passport-full.bin`, 5,929,664 bytes
- **Community submission**: `community-8e228d9f` (category games)

## What it does

- **Boots into the reader**: no test menu — the title screen is the front page.
- **The whole script on the device**: 101 chapters, 53,190 dialogue lines and about
  1.1 million characters packed into the firmware; nothing is downloaded at runtime.
- **Five routes, five endings**: the route is decided by affection flags set by the
  earlier choices, exactly as the source port's content package describes it.
- **Auto-read and fast-forward**: DOWN (hold) starts auto-read (the screen stays
  awake while it runs) and UP (hold) fast-forwards while held.
- **Saves**: one automatic slot written on every scene change plus five manual slots.
- **Chapter readout**: the corner shows the script's own chapter number (for example
  `4-7`) and the transition card shows the same number when a chapter changes.
- **Idle behaviour**: 60 s dims the backlight, 180 s turns it off and 420 s enters
  deep sleep; any key wakes the device. Auto-read and fast-forward do not count as
  idle time.

## Interaction

Three keys drive the whole app.

- **UP / DOWN (short)**: advance the dialogue. A press while the typewriter is still
  running completes the sentence; long sentences paginate and DOWN turns the page.
- **UP (hold)**: fast-forward until released.
- **DOWN (hold)**: toggle auto-read. An `auto` marker appears bottom-right; each line
  waits briefly after finishing and then advances. Any other key stops it.
- **OK (short or hold)**: menu — save, load, jump to the next choice, auto-play,
  back to title.
- **At a choice**: UP / DOWN select and OK confirms. Auto-read is deliberately *not*
  switched off while a choice is on screen, so answering one resumes reading.
- Press timing is 180 ms for a short press and 500 ms for a long press, passed to the
  button component explicitly by the BSP.

## Presentation and content limits

**There is no character art and no event CG.** The upstream Mi Band port ships only
backgrounds and chibi SD illustrations, so a speaking character is shown as a name
plate and scenes are carried by the background plus an SD window. The script, its
branches, the choices and all five endings are complete; only those two image classes
are missing, and they are missing upstream rather than dropped here.

## Highlights

- **A compressed script pack instead of a script dump**: the 5.06 MB node JSON
  becomes a 1.45 MB pack whose chunks decompress one at a time (3 KB raw per chunk,
  a 4 KB buffer). Chunk size is set by RAM, not by taste — see the linked experience
  entry — and jump targets resolve through a label table instead of scanning chunks.
- **The boot self-check**: `sanoba_app_init` decompresses the first chunk and decodes
  its first line, then logs the decompressed size and the speaker, or the
  decompressor's error string plus the free heap and the largest free block. Three
  unrelated hardware-only defects all used to present as "the story ends
  immediately"; this check now makes that class of failure a one-line diagnosis.
- **Layout sized from the panel**: 240 × 320 portrait with a full-width background, a
  240 × 144 SD window in the upper third and a five-line text band. Backgrounds decode
  straight into the canvas, and an SD image decodes into the row range it occupies, so
  neither needs its own decode buffer.
- **Text and font come from one table**: the pack's character table is ordered by
  global frequency and doubles as the glyph order of the 16 px subset (3,458 glyphs),
  so a generated font cannot be missing a character the script uses.
- **Offline and reproducible**: both packers and the font generator run from the
  sources committed under `assets/sanoba-source/`; rebuilding either pack is
  byte-identical to what the firmware carries.
- **A serial debugging channel for behaviour, not only pixels**: `SANOBAPAGE`
  streams a full frame back over the console, `SANOBASHOT` renders a scene
  standalone, `SANOBAJUMP` moves the reading position, and `SANOBAAUTO` switches
  auto-read so display and sleep behaviour can be checked without touching the
  device.

## Source and credits

The script and art come from the Mi Band fan port
[`hrk666666/Sanoba-Witch-MiBand-10`](https://github.com/hrk666666/Sanoba-Witch-MiBand-10)
(a Vela quick app whose own upstream is the archived
[`futrw4v/Sanoba-Witch-MiBand-9Pro`](https://github.com/futrw4v/Sanoba-Witch-MiBand-9Pro)).
Its code is GPL-3.0; the artwork belongs to Yuzusoft and the Simplified-Chinese text
belongs to its fan-translation group. This branch re-implements the reader on the
ATRI reader's LVGL page system and packs the sources it ships with
(`assets/sanoba-source/`), crediting the upstream project; the material is kept for
personal study and technical exchange only.
