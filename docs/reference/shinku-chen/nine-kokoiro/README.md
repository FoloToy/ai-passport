<p align="right">
  <strong>English</strong> · <a href="README.zh_CN.md">简体中文</a>
</p>

# 9-nine- Kokoiro reader

A landscape 320×240 visual-novel reader for *9-nine- Kokoiro* (Episode 1) on the
AI Passport. The firmware carries the complete story, expression patches, CGs,
title visuals and two BGM tracks offline.

## Scale

- 17 chapters, 74 nodes and 8,049 dialogue frames.
- Five choice groups / nine options; two endings (the first-playthrough loop end
  and the second-playthrough true end).
- 15 state variables, including five permanent keys; 22 CG groups / 73 variants
  and 17 chapter-read marks.

## Controls

| Key | While reading | In lists and choices |
| --- | --- | --- |
| OK / UP / DOWN (short) | Advance immediately; long lines advance one page unit | Move / confirm |
| OK (hold) | Open the menu | Go back |
| UP (hold) | Fast-forward one sentence per 50 ms tick; stops at choices, endings and OP | - |
| DOWN (hold) | Toggle auto-read | - |

## Implementation notes

- Thirty manual save slots store the node, frame index, text page and current-run
  variables in sparse CRC32-checked records bound to the pack identity.
- Permanent progress (`loop_my`, `loop1`, `true`, `clear_true`, `titleBg`) lives in
  a separate NVS record; loading, rewinding or a chapter preview never rolls a
  completed ending back.
- The 320×180 stage is decoded internally at 256×144 and shown at 1.25×, with a
  60 px dialogue panel below it.
- The gallery plays the opening from the local pack: 300 frames at 3 fps from a
  599-frame / 6 fps source, with the original track.

## Distribution

- Branch: `feature/nine-kokoiro-20261125`
  (<https://github.com/Shinku-Chen/ai-passport/tree/feature/nine-kokoiro-20261125>)
- Release: `v1.0.0-nine-kokoiro`
  (<https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-nine-kokoiro>)
- Community play: <https://ai-passport.folotoy.cn/plays/9-nine>
- Firmware: `FoloToy-AI-Passport-full.bin`, **5,405,040 B** (merged image)

## Upstream

- Source: [`xuanyide01/9-nine-rw5`](https://github.com/xuanyide01/9-nine-rw5),
  pinned at `6a8907a5ae576f4377abe4b74f15bd9af13efe07`.
- Framework: `feature/galgame-framework` in this fork.
