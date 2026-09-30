<p align="right">
  <a href="packing-a-visual-novel-into-8mb-no-psram.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Packing a Visual Novel into 8 MB with No PSRAM

Captured while packing the **Senren \* Banka** reader (2026-09-27): a full-length
visual novel — script, backgrounds, sprites, event illustrations — that has to
decompress and display out of an 8 MB flash part with no PSRAM and only single
digits of kilobytes of free heap. The numbers and the trade-offs are reusable for
any asset-heavy application on this board.

> **Verification status.** Measured on one AI Passport (ESP32-C3 revision v1.1,
> 8 MB flash, no PSRAM) with the reader validated on device: the whole script was
> read through, choices, saves and chapter jumps were exercised. The pack sizes,
> ratios and free-heap figures below are device and tool output, not estimates.
> Other titles, other flash parts and long-session memory behaviour are **not**
> covered.

## The budget decides everything

The factory partition is 8,323,072 bytes and the application itself uses
7,932,112 of them, so the data has to live inside the image rather than beside it:

| Part | Size |
| --- | --- |
| Image pack (786 entries: 93 backgrounds, 140 event CGs, 186 CG variants, 28 effects, 216 SD images, 123 sprites) | 5,263,756 bytes |
| Script pack (112 chunks, 886 blocks, 56.8 % of raw) | 1,432,148 bytes |
| Chinese glyph subset (16 px, 3,492 glyphs, sorted by code point) | — |

The figure that shapes every other decision is the one printed after boot: **the
largest free block is about 7.7 KB** (with roughly 10 KB free in total). Anything
that wants a big buffer has to be redesigned, not optimised.

## The decompressor sets the buffer budget

The obvious route is the ROM's inflate, and it was tried first. It failed on a
1,873-byte block that should expand to 2,963 bytes: the ROM path is built for
streaming with a 32 KB sliding dictionary, which this part cannot afford to
reserve, and the non-wrapping mode that would avoid it is not usable from the ROM
entry point here. A small non-wrapping inflate was written instead, which expands
one block into one buffer and needs no dictionary at all — the reason the block
ceiling below exists.

## Small blocks are cheaper than a big buffer

- **Script**: packed as blocks with a **3 KB ceiling and at most 128 records per
  block**. One 4 KB buffer serves the whole reader; jump targets resolve through
  chunk and block tables instead of scanning. The price is measured: the same
  text as a single stream would be roughly **197 KB smaller**, which was worth
  paying to keep the buffer at 4 KB.
- **Images**: a chunked palette format instead of a single PNG — geometry header,
  a palette, an alpha table and per-block lengths, with each block zlib'd
  separately, blocks capped at 3 KB and decoded with a 2 KB row buffer. Nothing in
  the image path needs a full-frame decode buffer, because backgrounds are decoded
  straight into the display canvas and a sprite is decoded into only the rows it
  occupies.
- **Event illustration variants** are stored as sparse mask patches against a base
  picture (1 bpp mask plus scan-order pixels), and each variant keeps whichever
  encoding — patch or full JPEG — is smaller.
- **Sprites** are stored per pose rather than per expression, bottom-aligned, at
  device width; **SD illustrations** collapse each alias group to the
  reference-weighted centre frame. That last one is a deliberate lossy trade-off
  (167 aliases, a mean difference of 21.4/255 against the dropped frames) with
  packer flags to keep every frame instead.

## Spend the pixels where the user looks

The source artwork is a 336 × 480 portrait composition. On a 240 × 320 portrait
screen it is a 6.7 % crop; the same art in landscape would have been a 47.5 %
crop. Packing for the device's own geometry — 240 × 320 backgrounds and event
illustrations, sprites at device width — is what made that crop cheap, and it also
removed every runtime scale and rotate step from the reader.

## Two failures that looked like nothing happening

Both of these cost real debugging time, and both were the same shape: a fixed-size
limit that silently refuses instead of failing loudly.

- **A save that never saved.** The save blob buffer was declared as 32 bytes while
  the encoder needs about 110 bytes for a typical record (a 10-byte header, 64
  flag bytes and four length-prefixed names, up to 336 bytes worst case). The
  encoder returned 0 by design, the store function returned false, and every
  auto-save and manual save was dropped for the whole bring-up — with the game
  behaving perfectly otherwise. Fix: size the buffer from the format (384 bytes
  here) and bump the format version when the layout changes.
- **A task that never existed.** A debug console task was created with an 8 KB
  stack while the largest free block at that point was 7,680 bytes, so the
  creation failed and printed one warning line at boot. The application was
  healthy and the serial channel simply never answered. Fix: create the task
  early, size it below the largest free block, and keep the failure line visible.

The habit that catches both is to make the state visible on the serial channel:
print the free heap and the largest block after boot, print a state line for the
reader (position, chapter, title, idle counters) on request, and add a boot
self-check that decompresses the first script chunk and prints its size and first
line. "Nothing happened" then becomes a number.
