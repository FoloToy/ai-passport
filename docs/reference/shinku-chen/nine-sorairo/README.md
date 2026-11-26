<p align="right">
  <strong>English</strong> · <a href="README.zh_CN.md">简体中文</a>
</p>

# 9-nine- Sorairo reader

A landscape 320×240 visual-novel reader for *9-nine- Sorairo* (Episode 2) on the
AI Passport. The firmware carries the story, art, music and the complete opening
offline, including a dedicated gallery entry for the opening.

## Scale

- 56 story nodes, 10,385 text frames and 21 chapters.
- Four choice groups / seven options and two ending records.
- 1,102 images, 22 CG groups / 72 variants and an opening of 623 frames at 5 fps
  (124.66 s).
- Audio is 12 kHz mono IMA ADPCM; the title and opening tracks are included.

## Controls

| Key | While reading | In lists and choices |
| --- | --- | --- |
| OK / UP / DOWN (short) | Advance; typing completes first, long text advances a page | Move / confirm |
| OK (hold) | Open the menu | Go back |
| UP (hold) | Fast-forward; choices and endings stop it | - |
| DOWN (hold) | Toggle auto-read | - |

The gallery collects CG, story recall, chapters, music and an **Opening** row.
The opening can be watched without clearing the story; natural completion, a
short press or a long press returns to the same row. Gallery browsing stays
silent, and returning to the title restores the title music.

## Implementation notes

- The opening is played from the local pack as 623 frames at 5 fps (124.66 s),
  without a full-screen decode buffer.
- Three manual save slots, history, font size and pagination are available from
  the in-game menu.
- The reader keeps the framework's page/key/persistence contract and replaces
  only the title's content and reading layer.

## Distribution

- Branch: `feature/nine-sorairo-20261125`
  (<https://github.com/Shinku-Chen/ai-passport/tree/feature/nine-sorairo-20261125>)
- Release: `v1.0.0-nine-sorairo`
  (<https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-nine-sorairo>)
- Community play: <https://ai-passport.folotoy.cn/plays/9-nine-2>
- Firmware: `FoloToy-AI-Passport-full.bin`, **7,266,952 B**, **87.31%** of the
  partition used, **1,056,120 B** free

## Upstream

- Source: [`xuanyide01/9-nine-rw5`](https://github.com/xuanyide01/9-nine-rw5),
  pinned at `6a8907a5ae576f4377abe4b74f15bd9af13efe07`.
- Framework: `feature/galgame-framework` in this fork.
