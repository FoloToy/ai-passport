<p align="right">
  <a href="lvgl-pool-and-glyph-coverage.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Sizing the LVGL Pool and Proving CJK Glyph Coverage

Captured while releasing the *Starry Sky Railroad and Shiro's Journey* reader.
Two failures on a text-heavy interface cost real debugging time and both are cheap
to prevent: an LVGL memory pool sized for a demo screen instead of the worst page,
and a CJK font subset that is generated from data but not checked against
everything that can reach the screen. Neither produces an assert or a clear error —
one corrupts the UI, the other draws blank boxes.

## The pool must be sized for the worst page, not the average

- The repository default (24 KB, sized for the demo UI) is not enough for a reader
  with five pages and four list screens: about 90 live objects (list pages with
  seven rows × two labels, plus full-screen pages). Opening the "about" page — a
  long text with a scroll container — failed to allocate, and LVGL 9 does not
  assert on that: the whole interface came up corrupted.
- Measured working value here: **56 KB** (`CONFIG_LV_MEM_SIZE=57344`). The runtime
  log then reports a 52,372-byte pool with about 32 KB in use at the title screen,
  so the headroom is real and the failure mode is gone.
- The configuration trap: **LVGL 9 renamed the key**. `CONFIG_LV_MEM_SIZE_KILOBYTES`
  is deprecated; the authoritative key is `CONFIG_LV_MEM_SIZE` in bytes. Setting
  only the old one leaves the default pool in place and prints a warning that is easy
  to scroll past — the symptom then looks like a UI bug, not a configuration bug.
  Print the pool at boot (`lv_mem_monitor()`) and read it back: total, used, free,
  largest free block and fragmentation.

## Proving a CJK subset covers the screen

- The subset is generated from data (the whole script) plus the interface strings —
  2,708 code points from Noto Sans SC here, as a 4 bpp plain bitmap font (about
  0.32 MB of flash, decoded one glyph at a time, no RAM cost).
- Because the inventory comes from data, the gate must also scan the **sources**.
  A character that appears only in a code comment once failed the check
  (U+5E27) — strict, but the right direction: the cost of adding one glyph is a few
  bytes, the cost of a missing one is a box on a player's screen.
- Regenerating and byte-comparing the generated C file against a fresh
  rasterization (the generator re-reads what it wrote and compares every glyph)
  catches generator drift, and the same check keeps the subset from silently
  shrinking when the script changes.
- If the packer and the font generator share the script, run both in the same gate:
  a new chapter without a regenerated font is exactly the case where the pack passes
  and the text turns into boxes.

## Related

- [Y2Lin's meter UI entry](../y2lin/meter-ui-smoothing-and-layout.md) — LVGL pool
  exhaustion as a cause of a white screen, from the UI side.
- [`asset-pack-field-drift.md`](asset-pack-field-drift.md) — the other "generated
  data must be verified on both sides" lesson from the same release.
