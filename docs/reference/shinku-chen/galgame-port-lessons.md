<p align="right">
  <strong>English</strong> · <a href="galgame-port-lessons.zh_CN.md">简体中文</a>
</p>

# Galgame Reader Porting Notes

Twelve visual novels were ported onto the AI Passport with the same reader
framework: *anemoi*, *Steins;Gate*, *Steins;Gate 0*, *CLANNAD*, *Summer Pockets
REFLECTION BLUE*, *Mahou Shojo no Majo Saiban*, *PARQUET*, *NianNiang Love
Story*, *Galgame Master*, *eden\**, *Stella of the End* and *Paper no Mahou
Tsukai* (the latter four in their Chinese releases: *Xiang Chengwei Galgame
Lingyu Dashen*, *Zhishang de Mofa Shi*, *NianNiang Lian'ai Gushi*). This document records the requirements they share, the capabilities the
framework provides, the defects that only appear on real hardware, and the
checklist used before shipping. It is written for the next port, not as a
changelog.

The device is an ESP32-C3 with 8 MB of flash and **no PSRAM**. Every constraint
below follows from that.

## Requirements

**Three keys, one meaning each.** The device has UP, DOWN and OK and nothing
else, so the semantics have to be identical in every port:

- Short OK advances one page; when the page is still typing, it completes the
  page instead of skipping it.
- Long OK opens the menu. Every secondary page (menu, saves, settings, history,
  gallery, chapters, about, CG viewer) must be exitable, and long OK is the
  universal "back".
- Long UP fast-forwards while held; releasing stops it.
- Long DOWN toggles auto-read.
- In lists and choices, short UP/DOWN move the cursor and short OK confirms.

**Dialogue panel.** The panel is flush with the bottom edge, spans the full
width, and is translucent so art stays visible behind it. The body text starts
4 px below the panel's top edge, the speaker name sits above the panel edge, and
the page counter sits outside the panel (or in its own strip) so it never
overlaps the body.

**Body text width.** Body text is laid out at a fixed pixel width and paginated
by a unit count derived from the font: full-width characters count 2 units,
half-width 1. A line must never wrap because a hanging punctuation mark pushed it
one character past the width — leave one character of slack per line.

**Idle policy.** Reading without input dims the backlight after 60 s (90 s in
*summer* and *galgod*, whose first pages are long), switches the screen off after
300 s, and enters deep sleep after 1800 s. Any key wakes the device. Auto-read
and fast-forward are **not** idle: an unattended auto-read run must keep the
screen on.

## Capabilities

- **Packed content.** Scripts, images and fonts are packed into one or two
  binary blobs that are embedded in the application assets, so a plain checkout
  builds a complete, playable title with no network and no SD card.
- **On-demand decoding.** Only the image the current page needs is decoded, into
  a canvas sized for the screen; JPEGs are decoded with the ROM decoder, sprites
  and overlays are lossless RGB565 plus a 1 bpp mask.
- **Banded rendering.** Because a full 320x240 RGB565 canvas is 153,600 B and the
  largest contiguous heap block on a real device is often 8-30 KB, art is drawn
  in horizontal bands: each band is decoded straight into its own buffer, so
  memory never scales with the screen.
- **Instant fast-forward.** Fast-forward renders the whole page immediately
  instead of running the typewriter at high speed; auto-read keeps the typewriter
  and pauses after the page completes.
- **Standard screens.** Save/load with several manual slots plus an automatic
  slot, gallery with unlockable CGs, chapter list, history (scrollable, wraps),
  settings (type speed, font size, panel opacity, idle behaviour) and an about
  page.
- **Shared reader layer.** Most ports share the `main/gal/` reader and only add
  their pack format and art pipeline; *galgod* and *manosaba* carry their own
  reader/UI layer under `main/`.

## Pitfalls

Each entry is symptom, root cause, fix. All of them were found on hardware, not
in the host tests.

### 1. A single-line label wraps, or disappears entirely

**Symptom.** A list row or the speaker name wraps onto a second line, or the name
plate becomes empty although the string is set.

**Root cause.** In LVGL 9 `LV_LABEL_LONG_DOT` is *multi-line with ellipsis*, not
single-line: it wraps first and ellipsizes the last line. The single-line variant
is `LV_LABEL_LONG_MODE_DOTS`. Combining `LV_LABEL_LONG_MODE_CLIP` with
`LV_SIZE_CONTENT` for the width makes the layout compute a width of 0, so the
object exists, holds the text and shows nothing.

**Fix.** Use `LV_LABEL_LONG_MODE_DOTS` for single-line labels that must show an
ellipsis, `LV_LABEL_LONG_WRAP` only for the history page, and give clipped rows
an explicit width (usable width minus the value column). Never pair `CLIP` with
`LV_SIZE_CONTENT`.

### 2. Colours are swapped: faces look cyan, red images look purple

**Symptom.** Skin tones come out cyan and backgrounds that are mostly red look
purple; images with little red look almost correct.

**Root cause.** tjpgd is configured with `JD_FORMAT 0`, whose output is **B, G,
R**, not R, G, B. Packing those bytes straight into RGB565 swaps red and blue.
The visible damage depends on how much red the picture contains, which is why
only "some" CGs looked wrong.

**Fix.** Swap the channels while writing the canvas (`bgr_to_rgb565()`), or set
the decoder's format to RGB. Ports that had the swap from the start looked fine
and hid the bug for a long time.

### 3. The screen goes black after setting an image

**Symptom.** A page that should show a background shows a black rectangle, and
the following pages stay black.

**Root cause.** `lv_image_set_src()` resets the object's size to the content
size. When the descriptor is not recognised (wrong header magic, wrong colour
format, zero stride) that content size is 0x0, so the object collapses.

**Fix.** Set the source first, then set the size explicitly to the art area, and
verify the descriptor header (magic, colour format, width, height, stride) before
handing it to LVGL.

### 4. A vertical half-transparent line on the right edge

**Symptom.** A thin scrollbar-looking line appears along the right edge of the
art area.

**Root cause.** The bands that make up the art are stacked as children of the
root object. If the sum of their heights exceeds the screen height (for example
6 x 56 = 336 > 320), the root becomes scrollable and LVGL draws a scrollbar.

**Fix.** Clear `LV_OBJ_FLAG_SCROLLABLE` on the root and clamp the last band's
height so the stack is exactly the screen height.

### 5. The same image appears three times, stacked

**Symptom.** Instead of one background the screen shows the same picture repeated
in three horizontal slices.

**Root cause.** Every band shared one pixel buffer and one image descriptor. All
descriptors therefore pointed at the buffer that was written last, so all bands
displayed the final band's content.

**Fix.** One buffer **and** one descriptor per band, each bound to its own
`lv_image` object once at startup and never rebound.

### 6. Large canvases fail to allocate on the device

**Symptom.** `heap_caps_malloc` of 153,600 B (a full 320x240 RGB565 canvas)
returns NULL on hardware while the host test passes; falling back to "scale the
whole image" then produces a black block or a missing band.

**Root cause.** About 187 KB of heap is available in total, but the largest
contiguous block is usually only 8-30 KB because Wi-Fi/BT stacks, LVGL pools and
task stacks fragment the DRAM.

**Fix.** Draw in bands, size each band from the largest block actually observed
(e.g. 4 x 80 or 5 x 64 rows), and probe from large to small with an explicit
fallback plus a log line. Never design a path that needs a >32 KB allocation.

### 7. Rotation deadlocks or shows a white screen

**Symptom.** Switching the panel orientation at boot hangs, or the first frame is
blank white.

**Root cause.** `lv_display_set_rotation()` (and the BSP's landscape helper) touch
LVGL state and the panel. Calling them without the LVGL lock races the LVGL task.

**Fix.** Hold `bsp_lvgl_lock()` around the call (and around any follow-up layout
work), then release. Orientation is a boot-time decision, not a runtime toggle.

### 8. `Stack protection fault` in the input task

**Symptom.** After a few page turns the device panics with
`Stack protection fault, Detected in task "xxx_input"`.

**Root cause.** The input task called straight into LVGL refresh and rendering
while holding only a 4096-byte stack.

**Fix.** Raise the input task stack to 8192 bytes, or move the heavy work into the
LVGL task and let the input task only enqueue events.

### 9. After long OK the next OK press is ignored, or the menu skips a step

**Symptom.** Holding OK opens the menu and the menu immediately advances one row;
or, the other way round, the first short OK inside a list does nothing and choices
become unselectable.

**Root cause.** The button driver emits an extra CLICK when a long press is
released. Swallowing every CLICK after a LONG fixes the first symptom and creates
the second: the flag stays set and eats the next genuine press.

**Fix.** Swallow the CLICK only inside a bounded window (about 400 ms after the
LONG), clear the flag on PRESS so a new gesture always starts clean, and clear it
when the window expires.

### 10. Reading a string from the pack hangs the device (WDT)

**Symptom.** Entering a certain page triggers a watchdog reset with no panic
message; the same page works in the host test.

**Root cause.** Strings inside the pack are length-prefixed and not NUL
terminated. Passing such a pointer to `lv_label_set_text()` makes LVGL scan
flash until it happens to find a zero byte.

**Fix.** Validate the length against the pack boundary, copy into a bounded
buffer and NUL terminate before handing the string to LVGL.

### 11. The version shows `<hash>-dirty` even though the source is committed

**Symptom.** `esp_app_desc.version` reports a git describe string instead of the
release version.

**Root cause.** ESP-IDF reads `PROJECT_VER` **before** `project()` runs; a
`set(PROJECT_VER "1.0.0")` placed after `project()` is ignored and the build falls
back to `git describe`.

**Fix.** Set `PROJECT_VER` before `include($ENV{IDF_PATH}/tools/cmake/project.cmake)`
and verify the value in the built `esp_app_desc`, not only in
`project_description.json`.

### 12. Sprites float above the panel, or one character appears three times

**Symptom.** A full-body sprite leaves a 20-30 px gap above the dialogue panel, or
one character is drawn three times side by side.

**Root cause.** The baseline was computed from the source image's pixel height
while the decoder returns a scaled height; and the pack gives the same character
several slots (one per expression) that sometimes point at three different image
ids holding the same JPEG, so de-duplicating by id misses them.

**Fix.** Place the sprite from the **decoded** height (`y = panel_top - h`), and
de-duplicate in the same frame by content: same width and height, and JPEG length
within about 160 bytes, means the same body — keep the last slot only.

## Branch and release map

All twelve ports live in `Shinku-Chen/ai-passport` as separate branches; each
one is built from its own worktree and released with its own annotated tag. The
branch names all end in `-landscape` for historical reasons, but **nianniang is
portrait 240x320** — the suffix is not a description of the layout for that one
title.

| Game | Branch | Tag | Layout | Flash use | Free |
| --- | --- | --- | --- | --- | --- |
| anemoi | `feature/port-anemoi-landscape` | `v1.0.0-anemoi` | 320x240 | 99.01% | 82,224 B |
| steins | `feature/port-steins-landscape` | `v1.0.0-steins` | 320x240 | 97.35% | 216,720 B |
| steins0 | `feature/port-steins0-landscape` | `v1.0.0-steins0` | 320x240 | 95.47% | 376,784 B |
| clannad | `feature/port-clannad-landscape` | `v1.0.0-clannad` | 320x240 | 97.92% | 173,056 B |
| summer | `feature/port-summer-landscape` | `v1.0.0-summer` | 320x240 | 91.86% | 672,080 B |
| manosaba | `feature/port-manosaba-landscape` | `v1.0.0-manosaba` | 320x240 | 94.77% | 428,720 B |
| parquet | `feature/port-parquet-landscape` | `v1.0.0-parquet` | 320x240 | 86.29% | 1,141,456 B |
| nianniang | `feature/port-nianniang-landscape` | `v1.0.0-nianniang` | **240x320** | 21.01% | 6,574,544 B |
| galgod | `feature/port-galgod-landscape` | `v1.0.0-galgod` | 320x240 | 39.48% | 5,037,248 B |
| eden | `feature/port-eden-landscape` | `v1.0.0-eden` | 320x240 | 93.54% | 538,000 B |
| stella | `feature/port-stella-landscape` | `v1.0.0-stella` | 320x240 | 99.20% | 66,992 B |
| paper | `feature/port-paper-landscape` | `v1.0.0-paper` | 320x240 | 75.56% | 2,033,760 B |

Figures are the application image against each branch's own `factory` partition.
**stella (66,992 B), anemoi (82,224 B) and clannad (173,056 B)** are close enough
to the limit that any further change must start with a flash budget check.

## Acceptance checklist

Run this on hardware before tagging a release.

- Boots to the title screen with no test menu and no network.
- Text renders in the subset font with no missing glyphs; every line of the first
  chapter is readable.
- Art shows correctly: background, sprite baseline, CG colour order, no black
  blocks and no repeated bands.
- Orientation matches the port (320x240 or 240x320) and is not flipped.
- Short OK advances, long OK opens the menu, long UP fast-forwards, long DOWN
  toggles auto-read; no double advance after a long press.
- Every secondary page can be left with long OK, including the last page of a
  list and the about screen.
- Save, load, gallery, chapter list and history all work, and a restored save
  lands on the same line.
- Fast-forward advances continuously with no per-line stall; auto-read keeps the
  screen on and stops at choices.
- Idle: the backlight dims at 60 s (90 s where configured), the screen turns off
  at 300 s, deep sleep happens at 1800 s, and a key press wakes and restores the
  session.
- No panic, no watchdog reset and no `Stack protection fault` during a full
  chapter read.
- The built `esp_app_desc` reports the release version, not a git describe string.

Assisted-by: Cindy (https://cindy.app)
