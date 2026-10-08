<p align="right">
  <a href="lvgl-cjk-font-subsets.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# CJK Bitmap Font Subsets for LVGL 9

Captured after the **Saya no Uta reader** release (a landscape visual novel on the AI
Passport: 44 chapters, 3,828 dialogue lines, three endings). These are general
learnings about shipping Chinese text on this board, not fork-specific customization.

> **Verification status.** Every number below comes from this release's build tree and
> on-device runs: the released firmware embeds a 2,839-code-point subset at 16 px and
> 20 px, boots without a single warning, and renders the whole script. The
> `lv_font_conv` comparison was run locally on Node v24 with the same OTF and options.

## Do not ship a full CJK font

An ESP32-C3 with 8 MB of flash and no PSRAM cannot carry a full CJK face, and it does
not have to: the app knows every string it can ever display. Collect the character
inventory from two sources and rasterize only those glyphs:

1. Non-ASCII characters inside string literals of the app's own sources (scan the
   `.c`/`.h` files instead of trusting a comment or a font request).
2. Every character of the content itself (script/chapter data), including speaker names.

Two subsets (16 px for body text, 20 px for the "large" setting) covering 2,839 code
points cost about 0.94 MB of flash together and no static RAM, because LVGL reads glyph
bitmaps straight out of flash.

## cmap must match LVGL's lookup, not the font's

LVGL's `get_glyph_dsc_id()` gives each cmap entry a different meaning, so a cmap that
looks valid can still resolve nothing:

- `LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY`: contiguous code points,
  `glyph_id = glyph_id_start + (cp - range_start)`.
- `LV_FONT_FMT_TXT_CMAP_SPARSE_TINY`: binary search in a `unicode_list` of offsets
  relative to `range_start`; `glyph_id = glyph_id_start + index`.
- `LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL`: needs a per-code-point
  `glyph_id_ofs_list`; passing `NULL` for a sparse set does not degrade, it crashes.

The layout that works for a subset built from a few thousand scattered code points is
one dense `FORMAT0_TINY` run for ASCII plus one `SPARSE_TINY` entry for everything
else — the same shape LVGL's own built-in fonts use.

## `lv_font_conv` wrote corrupt bitmaps under current Node.js

With the last release (1.5.3, 2021) running on Node v24, the same OTF and the same
options produced **byte-identical** output with and without `--no-prefilter`. Decoded
with LVGL's PLAIN reader those bytes are noise, while LVGL's own Montserrat fonts decode
correctly with the same decoder — so the reader was not the problem. On the device the
symptom is every label showing garbled glyphs, which is easy to misread as a font
selection or encoding bug.

Rasterizing with Pillow/FreeType and emitting the bitmap format directly removes the
toolchain from the trust path, and the result can be verified instead of eyeballed:
re-parse the generated C file, decode every glyph exactly the way LVGL will, and compare
it pixel by pixel against the rasterization. Fail the build on any mismatch.

## Details that turn into visible bugs

- **Bitmap packing.** PLAIN format (`bitmap_format = 0`), 4 bpp, high nibble first, each
  glyph starting on a byte boundary. Any other assumption silently shifts every glyph.
- **Line height is a layout parameter.** Set `line_height = pixel size + extra_leading`
  to match the screen's fixed line budget (16 px + 3 gives 19 px lines, four per page).
  Using the font's natural line height (Noto Sans SC measures about 24 px at 16 px)
  silently crops the last line of every page.
- **Whitespace needs glyphs.** A script that indents paragraphs with the full-width space
  (U+3000) needs that code point in the subset as a blank glyph with a full-width
  advance. Excluding it as "just a space" makes LVGL fall back to the missing-glyph box,
  which readers see as a row of squares in the middle of the text.
- **Keep an inventory file.** Emit the code-point inventory next to the fonts, let a
  `--check` mode fail the validation gate when a new UI string or content update needs a
  glyph the subset lacks, and let the inventory also act as an *input* so regenerating
  from a smaller content variant does not drop glyphs another variant still uses.
- **Do not commit the source font** when its license does not allow redistribution.
  Record the family, license, download source, regeneration command, and Flash impact in
  the asset documentation instead.
