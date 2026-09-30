<p align="right">
  <a href="vn-script-pack-budget-and-failure-modes.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Packing a Visual-Novel Script for a No-PSRAM Board

Experience from the [Sanoba Witch reader](sanoba-witch/README.md). The port reads a
5.06 MB script (101 chapters, 61k nodes, 1.12M characters of dialogue) plus 107
backgrounds and 292 SD illustrations, entirely from Flash, with a 240 × 320 portrait
panel and no PSRAM. Three defects on the way there were unrelated, but **all three
presented to the user as the same thing: the story ends immediately after you press
"start reading"**. That is what makes this worth writing down — the symptom hid the
cause, and the fix that made it visible (a boot self-check) is the reusable part.

## The pack, and why it is shaped this way

The script is stored as one binary with a section table: a character table ordered by
global frequency, name dictionaries, a scenario table, a label table, a route rule, a
chunk table and a blob of independent deflate blocks. Two decisions matter off-board:

- **Jump targets are resolved at pack time.** The source script jumps to label *names*
  that repeat across chapters; the packer resolves each jump to a
  `(scenario, chunk, node)` triple and stores it in a label table, so the firmware
  jumps in O(1) instead of decompressing chunks to look for a label.
- **Text is stored as indices into the character table**, not UTF-8 — 2 bytes per
  character — and that same table is the glyph order of the generated font subset.
  The font then cannot miss a character the script uses, and the subset stays at 3,458
  glyphs / 456 KB at 16 px 4bpp.

Measured packing result: 5.06 MB of node JSON → 1.45 MB, chunks decompressing to
≤ 3 KB each (1.99× on the record stream, 3.5× against the JSON).

## The RAM constraint is the chunk size

The chunk size is not a style choice. On this board, after the application has built
its canvas, LVGL pool and UI, the heap looks like this:

```text
I (1695) main: free heap 15468 bytes, largest contiguous block 7680 bytes
(the firmware logs this line in Chinese; the numbers are verbatim)
```

A decompression buffer must fit in **one** contiguous block, and that block is under
8 KB — so a 19.5 KB chunk (the natural size for a "one chunk per chapter" layout) can
never be allocated. The pack therefore splits at 3 KB of decompressed records and the
firmware uses a 4 KB buffer; the pack grows from 1.28 MB to 1.45 MB, which the 8 MB
Flash does not notice. Check the largest free block, not the free heap, before choosing
a chunk size on a no-PSRAM part.

## Failure mode 1 — the framing, not the compressor

The packer stripped the 2-byte zlib header and the Adler-32 checksum to store raw
deflate blocks. The firmware's inflate path had been written against a sibling project
that *keeps* the wrapper, so it was called with "parse the zlib header" set. Every
chunk then failed to decompress; the loader returned zero bytes.

The lesson is not "use one or the other" — both work — but that the framing is part of
the format contract: state it once in the packer's spec, and have the reader assert it.
A raw-deflate pack read as zlib produces exactly zero readable output and no
intermediate state to inspect.

## Failure mode 2 — a failed allocation that looks like "the end"

```c
if (!player_load_chunk(player, scn, player->chunk)) {
    return SANOBA_STEP_STUCK;
}
...
if (!player_step_position(player, scn)) {
    return SANOBA_STEP_ENDING;      /* ← out of memory lands here */
}
```

Chunk loading can fail for several reasons: a corrupt block, a bad table entry, or
`malloc` returning NULL. The loader returned 0 for all of them, and the state machine
treated "no more chunks" as **the story is over**. A 32 KB buffer against a 7.7 KB
largest block therefore looked like finishing the game the instant you pressed start.

Keep I/O and allocation failure on a different return path from "end of content", and
log the difference. On this board the warning that prints the free heap and the
largest contiguous block next to the failure is usually enough to name the cause.

## Failure mode 3 — the stack, and a config file that did not change

The same "ends immediately" symptom came back after the buffer was fixed, this time as
a real stack overflow: the boot self-check runs a full decode pass (chunk decompress →
record decode → pagination) from `app_main`, which overflows the default main task
stack of 3,584 bytes. Raising it in `sdkconfig.defaults` **did not take effect**,
because ESP-IDF only reads that file when `sdkconfig` is generated; an existing
`sdkconfig` keeps its old value and the build silently uses 3,584 bytes again. Moving
the file away and letting the build regenerate it produced the intended value:

```text
#define CONFIG_ESP_MAIN_TASK_STACK_SIZE 8192
```

Two things to remember: a decode pass during startup needs the main task stack sized
for it (the stack is freed when `app_main` returns, so this only costs boot RAM), and
when a `sdkconfig.defaults` change does not show up, check the generated `sdkconfig`
rather than re-reading your edit.

## The boot self-check that made all three visible

```c
/* decompress the first chunk, decode its first line, then either log it or log why */
/* the log text itself is Chinese; this is the shape of it */
ESP_LOGI(TAG, "<script self-check>: chunk 0 decompressed %u bytes, first line <%s> %u chars", ...);
ESP_LOGE(TAG, "<script self-check failed>: advance returned %d, chunk 0 decompressed %u bytes"
              " (inflate error: %s); free heap %u, largest block %u", ...);
```

It costs a few milliseconds at boot and it turns "the game ends immediately" into
either a passing line with real numbers or a failure line that names the
decompressor's own error plus the heap state. Both the framing bug and the allocation
bug were identified from this single line afterwards.

## Verifying behaviour, not just pixels

A serial screenshot channel (`SANOBAPAGE`) captures frames from the LVGL flush path,
which is enough for layout. Behaviour that depends on the *state machine and timers* —
here, "auto-read must not dim, blank or sleep the screen" — needs a way to enter that
state without hands. Small console commands cover it: `SANOBAJUMP <scenario> <step>`
moves the reading position and `SANOBAAUTO <0|1>` toggles auto-read, so a backlight
that must not change can be watched for two minutes with the log showing
a backlight transition line (idle, then restored / dimmed / off) only when one really
happens. Measured:
auto-read on → zero backlight changes in 135 s; auto-read off → dim at 60.0 s.

## Numbers to reuse

| Item | Measured |
| --- | --- |
| Script JSON → pack | 5.06 MB → 1.45 MB |
| Chunk size / buffer | ≤ 3 KB raw per chunk, 4 KB buffer |
| Heap after boot | 15 KB free, **7.7 KB largest contiguous block** |
| Font subset | 3,458 glyphs, 16 px 4bpp, 456 KB, table-ordered |
| Image pack (107 backgrounds + 292 SD) | 2.97 MB |
| Full firmware, both packs embedded | 5.93 MB merged image |
