<p align="right">
  <strong>English</strong> · <a href="README.zh_CN.md">简体中文</a>
</p>

# 9-nine- Yukiiro reader

A landscape 320×240 visual-novel reader for *9-nine- Yukiiro* (Episode 4) on the
AI Passport. The firmware carries the complete Episode 4 story, every branch and
ending record, the gallery and the opening offline.

## Scale

- 212 graph nodes and 12,665 text frames across 23 chapters.
- 47 choice nodes / 70 records (63 reachable under the source gates; seven are
  structurally unreachable).
- Two ending records (`loop1` mid-story completion and `true` terminal), 19 CG
  groups / 108 variants and 78 runtime variables.
- 668,498 UTF-8 bytes of text in 84 zlib blocks.

## Controls

| Key | While reading | In lists and choices |
| --- | --- | --- |
| OK / UP / DOWN (short) | Advance to the next complete page or line, even while typing or auto-playing | Move / confirm |
| OK (hold) | Open the menu | Go back |
| UP (hold) | Advance to the next whole sentence; one sentence per input tick, stop on release | - |
| DOWN (hold) | Toggle auto-play | - |

## Implementation notes

- Artwork and story chunks come from the upstream watch project's processed
  graph; the uncommitted original PC scripts are not rebuilt. PC voice and adult
  assets are not included (the watch source is already non-adult and zero-voice).
- Chapter previews and gallery playback never write automatic or permanent
  progress, and a long-press release never fires an extra short advance.
- The 320×180 stage keeps the source 16:9 composition, with the dialogue panel
  below it and the framework's three-key pagination contract.

## Distribution

- Branch: `feature/nine-yukiiro-20261125`
  (<https://github.com/Shinku-Chen/ai-passport/tree/feature/nine-yukiiro-20261125>)
- Release: `v1.0.0-nine-yukiiro`
  (<https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-nine-yukiiro>)
- Community play: <https://ai-passport.folotoy.cn/plays/9-nine-4>
- Firmware: `FoloToy-AI-Passport-full.bin`, **8,258,768 B**, **99.23%** of the
  partition used, **64,304 B** free

## Upstream

- Source: [`xuanyide01/9-nine-rw5`](https://github.com/xuanyide01/9-nine-rw5),
  pinned at `6a8907a5ae576f4377abe4b74f15bd9af13efe07`.
- Framework: `feature/galgame-framework` in this fork.
