<p align="right">
  <strong>English</strong> · <a href="README.zh_CN.md">简体中文</a>
</p>

# Tenshi☆Sousou RE-BOOT! reader

A portrait 240×320 visual-novel reader for *Tenshi☆Sousou RE-BOOT!* on the AI
Passport. The firmware carries the complete Chinese-translated story, six
heroine routes and fifteen endings offline; no network or external files are
needed.

## Scale

- 61,436 script pages, 61,289 dialogue lines and 1,165,911 characters across 45
  chapter points.
- 13 choice points and 15 ending points: six heroine routes (Noa, Amane, Kurumi,
  Kaguya, Orie, Fumika), two BAD ENDs, one END and six return-to-title screens.
- 6.26 MiB resource pack; 16 px Chinese subset with 3,417 codepoints.

## Controls

| Key | While reading | In lists and choices |
| --- | --- | --- |
| OK (short) | Advance one line; completes a line that is still typing | Confirm |
| OK (hold) | Open the menu | Go back one level |
| UP / DOWN (short) | Advance one line | Move the cursor |
| UP (hold) | Fast-forward while held | - |
| DOWN (hold) | Toggle auto-read | - |

On the title screen UP / DOWN select, OK enters and a long OK powers the device
off. The menu covers continue, save, load, skip chapter and chapter select.

## Implementation notes

- The reader reuses the ATRI / Starry Sky Railroad LVGL page system and adds the
  original data-driven branching: 15 endings, five flag gates (11 rules / 54
  conditions), 18 forward and 38 back jumps, all read from `branch.json`. The
  build fails when a gate points at a choice that does not exist.
- A 150 KB RGB565 canvas has no separate decode scratch buffer. Backgrounds,
  event art, patches and sprites decode through the tjpgd callback directly into
  the canvas; sprites use a 1bpp mask blitted line by line from flash.
- The roughly 1.9 MiB script region (page table, text stream, choice table and
  branch table) stays mapped read-only; images use a 256 KB sliding
  `esp_partition_mmap` window and decode on demand.
- Host tests walk the complete page table and each of the six routes, including
  ending points, gates and the pagination layout.

## Distribution

- Branch: `feature/tsxx-reboot`
  (<https://github.com/Shinku-Chen/ai-passport/tree/feature/tsxx-reboot>)
- Release: `v0.1.1-tsxx-reboot`
  (<https://github.com/Shinku-Chen/ai-passport/releases/tag/v0.1.1-tsxx-reboot>)
- Community play: <https://ai-passport.folotoy.cn/plays/re-boot-galgame>
- Firmware: `FoloToy-AI-Passport-full.bin`, **8,135,520 B**, **97.75%** of the
  partition used, **187,552 B** free

## Upstream

- Source: [`hezdaaa/tsxxreboot-miband`](https://github.com/hezdaaa/tsxxreboot-miband)
  — the Xiaomi Band quick-app fan port, with no declared licence; the story and
  artwork belong to Yuzusoft and the original publisher.
- Reader baseline: the ATRI / Starry Sky Railroad LVGL page system in this fork.
