<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# limelight lemonade jam

A portrait visual novel reader for the AI Passport, ported from the Xiaomi Band
release of the commercial title *limelight lemonade jam*. Three keys read the whole
story offline: **230 chapter points, 68,229 dialogue lines, about 1.2 million
characters, 991 packed images and eight choice points**.

The upstream release is a touch application for Xiaomi's Vela OS and declares no
licence; this port re-implements the reading engine in C on LVGL, converts the
artwork and script into two packed images inside the firmware, and credits the
upstream project as their source.

## Publish information

- **Title**: `limelight lemonade jam — visual novel reader` (the localized title used for
the submission is recorded in the Chinese version of this page, `README.zh_CN.md`)
- **Description**: the submitted English copy:

  > The complete *limelight lemonade jam* visual novel, ported to AI Passport so the
  > whole story fits in three keys.
  >
  > Yukitaka Surfnami plays bass but never joined a band of his own, and he has no
  > other plan for his days. Then a street performance makes him stop: a girl named
  > Nagisa Himi is singing alone, with an amateur guitar and no band behind her.
  > From that evening on, his ordinary life starts to shine again.
  >
  > How to play: boot into the title screen and pick "Start". UP / DOWN advance a
  > line, and pressing while a line is still typing reveals it at once; holding UP
  > fast-forwards and stops on release, holding DOWN toggles auto-read, and the
  > screen stays lit while it runs. OK opens the menu: five manual save slots, one
  > automatic slot, chapter jump, a CG gallery and settings. The current chapter is
  > shown in the top left (for example 7-2) and the battery at the top right.
  >
  > Content: 230 chapter points, 68,229 lines of dialogue, about 1.2 million
  > characters and eight choice points — fully offline.

- **Category**: games
- **Cover**: `comm_cover.png` (PNG, 1152 × 1536, 3:4) — the upstream key visual
  supplied by the fork owner, cropped to the required ratio and not otherwise
  altered. Publish metadata only; the image is not committed in this repository.
- **Source**: <https://github.com/Shinku-Chen/ai-passport/tree/cindy/curious-babbage>
- **Release**: tag `v0.1.0-limelight` (`dc7154d`), merged image
  `FoloToy-AI-Passport-full.bin`, 7,608,464 bytes, built by the tag-triggered CI.
- **Community submission**: project 680, revision 1444, slug
  `limelight-lemonade-jam`, status `pending` when submitted.

## What it does

- **Title, body, choices, chapter jump, gallery, settings**: the pages reuse the
  ATRI reader's LVGL page system, so the three keys behave the same way across the
  fork's visual novels.
- **The story is read in pages**: the renderer lays a line out by character width
  (half-width counts as one unit, full-width as two) and paginates at five lines of
  26 units; the page indicator appears only for the 13 dialogue lines that need a
  second page.
- **Sprites sit at the bottom right of the screen, behind the dialogue band**: the
  art canvas covers rows 0–213, and the rows below that are drawn by a second LVGL
  image object that points into the decode buffer, then covered by the translucent
  band.
- **Chapters are shown as `X-Y`**: the upstream script carries 230 `[CHAPTERx-y]`
  markers; the in-game label and the chapter list both render them as `0-1`, `7-2`
  and so on, and a marker is never shown as a speaker name.
- **Saves and idle**: five manual slots plus one automatic slot written on every
  scene change; 60 s dims the backlight, 3 min turns it off, 7 min enters deep
  sleep, and auto-read and fast-forward keep the screen lit while they run.

## Data and toolchain

- **Artwork pack** (`main/limelight_data/limelight_pack.bin`, LLMPK001, 4.21 MiB,
  991 entries): backgrounds and CGs as 240 × 214 JPEG, sprites as alpha-cut RGB565
  with a 1 bpp mask, reference-counted cropping, and a name table keyed by
  "family + basename".
- **Script pack** (`main/limelight_data/limelight_script.bin`, LLSPK001, 1.81 MiB,
  68,229 entries in 273 blocks): UTF-8 text compressed per 250-entry block with raw
  deflate, which the ESP32-C3 ROM inflater expands at no firmware cost.
- **Font**: a 16 px / 4bpp subset (3,449 glyphs) generated from Noto Sans SC
  (SIL OFL 1.1) with the same metrics as the ATRI reader's font.
- **Tools**: `tools/limelight_material_pack.py`, `tools/limelight_script_pack.py`
  and `tools/limelight_lvgl_font.py`, all offline and covering pack-time text
  cleaning (see the experience entry below).

## What was verified

- **On hardware** (AI Passport, ESP32-C3, 8 MB): boot, the three keys including
  hold-to-fast-forward and auto-read, save/load, chapter jump, the CG gallery, the
  idle policy, and the serial capture used for publishing. Layout was checked by
  reading frames back from the device rather than by eye.
- **Host tests**: package headers, name/chapter/choice tables, the block cache, the
  player model over the real 68,229-entry script (every page join reconstructs its
  line), the image-layout math, and the packers' budgets and cleaning rules.
- **Not measured**: peak audio draw (the reader plays no audio), battery life in
  continuous auto-read, and read/write durability of the NVS save slots.
