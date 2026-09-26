<p align="right">
  <a href="lvgl-font-format-and-glyph-coverage.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# LVGL Font Format Traps and Glyph-Coverage Gates

Collected after releasing **ATRI Reader**, whose Chinese UI needed a 16 px font
subset of 2,770 code points (about 0.39 MB of flash, 4 bpp, uncompressed) built from
the script pack and the application's own strings.

## `FORMAT0_FULL` crashed LVGL 9.6; TINY and SPARSE_TINY did not

The first generated font used the largest layout (`render_mode=FULL`, i.e. a dense
glyph-descriptor table). Compiled and linked fine, and the string was valid UTF-8 —
but the device faulted inside the glyph lookup on the first Chinese label, before
any UI was drawn. Regenerating the same bitmap data as `FORMAT0_TINY` for ASCII plus
`SPARSE_TINY` (a sorted code-point array plus a matching index array) for the rest
fixed it with no behaviour change.

For an arbitrary CJK subset the sparse layout is also the cheaper one: the location
table only ever lists the code points that are actually used, so 2,770 glyphs do not
need a 20,992-entry dense table. Treat the format choice as part of the porting work
rather than a detail of the generator: verify it on hardware with a real Chinese
string before building the rest of the UI on top.

## Verify the emitted font, not the tool that emitted it

Two checks caught every font problem in this project, and both are cheap:

- **Per-pixel readback.** After writing the C source, re-parse it and compare each
  glyph's bits against the FreeType rasterisation that produced it. A generator bug
  then fails the build instead of showing up as a smudged glyph on the panel.
- **Coverage against the source.** Extract every non-ASCII character from the
  application's string literals and assert each one is present in the inventory file.
  Adding a new UI string with an unmapped character then fails the static gate,
  which is much cheaper than noticing a tofu box on the device.

Keep the character inventory as a committed text file so the coverage check needs no
font toolchain at gate time.

## Generator notes

- `lv_font_conv` 1.5.3 writes corrupt glyph bitmaps under current Node.js; on the
  device that shows up as a screen of noise. An in-repo generator that fails loudly
  is easier to trust than pinning an old Node.js.
- Instantiate the source font at the weight you want (`wght=400` from a variable
  font) and keep the `.ttf` out of the repository; commit only the generated C file
  and the coverage list.
- Log the LVGL pool usage at boot. The same project's About page exhausted the pool
  with the default size and left the whole screen garbled until reboot; raising it
  to 56 KB fixed it, and the boot log makes that visible before a user finds it.
