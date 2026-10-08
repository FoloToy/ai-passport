<p align="right">
  <a href="packed-visual-novel-data.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Packing a Visual Novel Into One Flash-Mapped Blob

Captured after the **Saya no Uta reader** release. General guidance for porting
script-and-art content to the AI Passport, not fork-specific customization.

> **Verification status.** Measured on this release's build and hardware: the committed
> pack is 2.78 MB (44 chapters, 473 scenes, 3,828 dialogue lines, 193 backgrounds, 75
> sprites), a 320 × 150 background decodes in 75-105 ms on the ESP32-C3, and the reader
> plays the whole script on the device.

## One little-endian blob, read straight out of flash

Do the parsing work once, on the host, and let the device read the result as memory
mapped from the application partition. The layout that worked:

```text
header   magic, version, total size, section count, section table
section  { type u32, offset u32, count u32, size u32 }
  TEXT     one UTF-8 blob; every string is addressed by (offset, length)
  NAME     speaker / choice / ending name table
  CHAPTER  id, first scene, scene count, first dialogue, dialogue count, next chapter
  SCENE    background, choice count, first dialogue, dialogue count, 2 choice targets
  DLG      text offset+length, name, sprite, flags, jump, argument
  BG       { offset, length, width, height } + JPEG bytes
  FG       { jpeg offset/length, mask offset/length, width, height } + bytes
  META     key=value provenance text
```

Offsets inside a section are relative to that section's own data area, which keeps the
loader a couple of pointer additions; there is no JSON parsing, no decompression and no
per-frame allocation on the device.

## Clean the script at pack time, not at runtime

Source scripts from other engines carry directives that are not prose. In this port the
script contained 31 sound-effect tags of the form
`<se id="1" src="…" mode="normal" loop="off">`; with no audio assets in the port they
were rendered as literal dialogue text until the packer stripped them. Rules that kept
the text clean:

- Strip the engine's own directives (sound, background, effect tags) while packing, and
  log how many were removed so a regression is visible in the build output.
- Normalize CJK compatibility ideographs (U+F900-U+FAFF) with NFKC; they are glyph
  variants of standard code points and only waste subset entries.
- Drop C0 control characters, keep `\n` and the full-width space that the script uses
  for indentation.
- Leave genuinely empty dialogue entries alone when they carry control flags: in this
  data three empty lines carry the ending transitions, so filtering "empty text" would
  have broken all three endings.

## Pre-crop the art so the device only decodes and composites

- Backgrounds are cover-cropped on the host to the exact art area (320 × 150 here) and
  stored as JPEG, so the device never scales or crops.
- Sprites are scaled to screen height once, cropped to the visible band, and stored as a
  JPEG plus a 1 bpp row-packed mask (`stride = (width + 7) / 8`); the device composites
  by mask. Keeping sprites only as tall as the visible area is what makes a 180 px-wide
  sprite fit a no-PSRAM decoder buffer.
- Decode lazily: this reader decodes only when the (background, sprite) pair actually
  changes, which is why 75-105 ms per decode is acceptable on a 160 MHz part.

## Make the pack traceable and reproducible

- Keep provenance in the pack itself (source repository, commit, converter version,
  geometry and JPEG quality parameters). It costs nothing and answers "which inputs
  produced this file" without keeping build notes.
- Commit the generated pack when the content license allows redistribution, so a plain
  `git clone` builds a playable firmware; regeneration then only needs the upstream
  content checkout. Record the regeneration command in the asset documentation.
- Treat the pack as data with a schema: a host test that opens the committed pack and
  validates every count, offset and mask length catches packer regressions without
  flashing anything.
