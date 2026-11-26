<p align="right">
  <strong>English</strong> · <a href="newer-galgame-port-lessons.zh_CN.md">简体中文</a>
</p>

# Porting the Newer Galgame Batch: Reusable Lessons

Collected while porting and publishing eight titles in late 2026: Tenshi☆Sousou
RE-BOOT!, Shinuyakata, Café Stella and the Reaper's Butterflies, Maitetsu ·
Niiroku Route, and the four 9-nine episodes. These are the rules that survived
more than one title; per-title budgets and quirks stay in the application
playbooks.

## Treat the watch port as the source of truth

- All eight ports derive from processed RW5 watch projects under
  `xuanyide01/*-rw5`, pinned to exact commits. The converted graph, engine and
  chunk codec are the authority; do not reconstruct uncommitted PC scripts.
- Keep unreachable source nodes and conditions unreachable. If a gate can never
  fire in the source graph, do not invent a route just to make the port look
  complete.
- The four 9-nine episodes share one upstream project and one reader family, but
  ship as separate firmware images. Reuse the reader, not the content package.

## Keep the reader contract identical across titles

- All three short keys advance immediately. While text is typing or auto-play is
  running, a press advances; it never merely finishes typing or exits auto and
  swallows the input.
- A long press fires only its long action. Releasing it never adds a short
  advance.
- Long UP fast-forwards whole sentences, skips typewriter and auto waits, and
  stops at choices, endings and the opening. It never chooses for the reader.
- Long DOWN toggles auto-play. A short key during auto leaves auto and advances
  in the same press.
- Returning from menus, save/load, settings or history restores the same text
  page, or the full choice list with its cursor.
- Chapter previews and gallery playback never write automatic or permanent
  progress, and never unlock endings or CGs.

## Budget the display as bands, not a full frame

- The common layout is a 320×180 stage plus a 60 px dialogue panel. Source art is
  432×243; keep its 16:9 composition instead of cropping it to fill the panel.
- Café Stella packs the stage at 224×126 and draws three 224×42 RGB565 bands,
  scaled to 320×180. Portraits and faces use a 2 bpp alpha mask (four levels);
  JPEG segment dedup and a frame-art cache keep repeated art out of the heap.
- TSXX uses a 150 KB RGB565 canvas with no decode scratch: tjpgd callbacks write
  directly into the canvas, sprites use a 1bpp mask, and the script region stays
  read-only while images use a 256 KB sliding `esp_partition_mmap` window.
- Kokoiro decodes the 320×180 stage internally at 256×144 and shows it at 1.25×.
- Measure the largest contiguous heap block before choosing a layout. A full
  RGB565 frame plus a decoder scratch buffer is usually the wrong answer on the
  no-PSRAM part.

## Separate run state from permanent progress

- Save records store the node, frame index, text page and current-run variables
  in a sparse, CRC32-checked record bound to the pack identity. Kokoiro uses 30
  manual slots; Maitetsu, Shinuyakata and Sorairo use three.
- Permanent keys live in their own NVS record. Loading, rewinding, chapter
  preview or gallery playback must not roll a completed ending back.
- A damaged save or progress record is reported on screen and never silently
  applied; the live state is kept.
- A chapter or CG preview is an isolated run. Reading from chapter 1 to the end
  inside a preview still does not award the ending; only a New Game run does.

## Audio and opening video

- IMA ADPCM at 4, 8 or 12 kHz, interpolated to 16 kHz on playback, keeps complete
  title themes and openings inside flash without replacing them with square
  waves. Maitetsu keeps the full 283.88 s title theme.
- Opening video is reduced in frames, not in story content: 599 frames at 6 fps
  become 300 at 3 fps in Kokoiro; Sorairo keeps 623 frames at 5 fps; Haruiro
  keeps 493 at 5 fps; Café Stella keeps 287 frames at 3 fps from a 5 fps source.
- Reuse the existing bands and stage for the opening. Do not allocate a second
  full-frame RGB565 buffer just for the movie.
- Gallery playback runs outside the live story VM: it must not advance the VM,
  write saves or unlock endings and CGs.

## Validation that catches port regressions

- Reconcile every dialogue frame, choice target, condition and ending reference
  against the source graph. Haruiro reconciled 10,659 frames and matched a
  10,674-step trajectory against the source `engine.js`.
- Host-test pack decoding, composition samples, the three-key state machine and
  the save/progress format. Real LVGL decoding and a 32-bit / 64-bit UI smoke
  catch layout and allocator problems that pure data tests miss.
- Keep device results separate from host results. Long-run power-loss behaviour,
  full-route device runs and audio quality need their own evidence; do not turn a
  successful build into a device-validation claim.

## What to copy, what not to

- Copy the source pinning, the reader contract, preview isolation, the banded
  display budget and the source-graph reconciliation.
- Do not copy a specific title's band count, slot count or frame rate without
  measuring. The reusable part is the budget method, not the number.
