<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Asunabi

A portrait visual-novel reader for the AI Passport. It carries a complete story that
was originally a Xiaomi Band quick app — 30 chapters, 4,649 dialogue lines and about
94,000 characters, from the first line to a single ending. The firmware replaces the
demo menu and boots straight into the title screen.

## The story

The student council president is Asuka Shishidou, known as the Ice Princess: in the
after-school corridor a teacher's call catches someone else's name, her blush gives
her away, and a notebook the protagonist happens to pick up is what puts him on her
radar. Thirty chapters carry the two from that first meeting to a single ending,
with chapter select for readers who want to jump straight to a favourite scene.

## Publish information

- **Title**: Asunabi
- **Description**: not submitted to the community market yet, so no cover image is
  recorded here. The Git release describes it as:

  > A portrait reader for a 30-chapter visual novel ported from a Xiaomi Band quick
  > app: start, continue, chapter select and settings; six save slots; text speed and
  > size; auto-play; one ending.

- **Category**: games
- **Cover**: none recorded (no community submission)
- **Source**: <https://github.com/Shinku-Chen/ai-passport>, branch `feature/asunabi-galgame`
- **Story and artwork**: <https://github.com/liuyuze61/Asunabi-miband>

## What it does

- **Boots straight into the story**: no test menu, no network, and no audio — nothing
  in this application plays sound, so the codec is left uninitialised.
- **Reading**: confirm advances a line; a line still being typed is revealed in full
  first; a line too long for the panel is paginated rather than clipped.
- **Auto-play**: long-pressing DOWN toggles it, a line advances 0.9 s after it
  finishes revealing, and any key press ends the mode.
- **Fast-forward**: holding UP advances while held and stops the moment it is
  released.
- **Saves**: six manual slots plus an automatic resume point, both in NVS; a slot is
  deleted by long-pressing confirm on it. Save positions include the page within a
  line.
- **Settings**: text speed (slow / medium / fast / instant), text size (16 px or
  20 px, with a live typing preview), and auto-play.
- **Chapter select**: jump to any of the 30 chapters from the title screen.
- **Drawn over the artwork**: a 30% tint over the title backdrop, a 50% plate behind
  the option list, and a 70% dialogue panel — the scene stays visible while near-white
  text stays readable, and the firmware never blanks the panel, so auto-play can run
  unattended.
- **Long press registers at 300 ms**: the BSP passes the press timing explicitly
  instead of using the button component's 1,500 ms default.

## Interaction

Three keys. The battery percentage is at the top right and degrades to `--%` when the
gauge cannot be read.

| Key | Reading | In a menu |
| --- | --- | --- |
| UP (short) | next line, or reveal the rest of the current one | move the selection up |
| UP (hold) | fast-forward while held | — |
| OK (short) | open the menu | activate the selected row |
| OK (hold) | — | leave the menu |
| DOWN (short) | scroll a line that runs past the panel | move the selection down |
| DOWN (hold) | toggle auto-play (0.9 s per line; any key cancels) | — |

## Assets

The story text and artwork are third-party content from the upstream Xiaomi Band
project, which declares no license. On this fork they are committed under
`assets/gal-source/`, so a clone builds the complete game; the pipeline in the
branch's `tools/gal/` packs them into a read-only 4 MiB `assets` data partition
(3.55 MiB used) and derives the CJK font subsets from the packed scripts. A build
without that tree still boots, with a placeholder pack instead of the story.

## Source

- **Branch**: [`feature/asunabi-galgame`](https://github.com/Shinku-Chen/ai-passport/tree/feature/asunabi-galgame)
- **Release**: tag `v0.1.0-asunabi` on the fork; merged image
  `FoloToy-AI-Passport-full.bin`, 7,917,142 bytes (application at `0x10000`, assets at
  `0x400000`)
- **Asset pipeline**: [branch `tools/gal/`](https://github.com/Shinku-Chen/ai-passport/tree/feature/asunabi-galgame/tools/gal)
- **Build**: `./tools/validate.sh --firmware`, after installing Pillow into the Python
  environment the ESP-IDF build uses; flash the merged image from `0x0`
