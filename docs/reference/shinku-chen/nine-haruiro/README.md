<p align="right">
  <strong>English</strong> · <a href="README.zh_CN.md">简体中文</a>
</p>

# 9-nine- Haruiro reader

A landscape 320×240 visual-novel reader for *9-nine- Haruiro* (Episode 3) on the
AI Passport. The firmware carries the complete non-R18 Chinese main line, both
endings, the gallery and the opening movie offline.

## Scale

- 78 graph nodes / 15 chapters and 10,659 dialogue frames.
- 13 choice nodes / 13 visible options and two ending milestones (branch and
  true).
- 23 state variables (10 persistent keys), 2,368 visual snapshots / 9,333 layers,
  18 CG groups / 100 variants and two BGM tracks.
- Opening: 493 frames at 5 fps (98.62 s).

## Controls

| Key | While reading | In lists and choices |
| --- | --- | --- |
| OK / UP / DOWN (short) | Advance; typing completes first, long text advances a page | Move / confirm |
| OK (hold) | Open the menu | Go back |
| UP (hold) | Fast-forward; choices and endings stop it | - |
| DOWN (hold) | Toggle auto-read | - |

## Implementation notes

- The source snapshot excludes the 11 R18 scenes and all voice tracks; the port
  neither reconstructs nor invents them. “Complete” means complete relative to
  that snapshot: every dialogue frame, choice target, both endings and every art
  reference are in the pack.
- All 10,659 frames were reconciled line by line, and the 10,674-step trajectory
  matches the source `engine.js` exactly.
- The reader keeps the framework's three-line 16 px / two-line 20 px pagination
  and its three-key interaction contract.

## Distribution

- Branch: `feature/nine-haruiro-20261125`
  (<https://github.com/Shinku-Chen/ai-passport/tree/feature/nine-haruiro-20261125>)
- Release: `v1.0.0-nine-haruiro`
  (<https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-nine-haruiro>)
- Community play: <https://ai-passport.folotoy.cn/plays/9-nine-3>
- Firmware: `FoloToy-AI-Passport-full.bin`, **8,127,984 B**, **97.66%** of the
  partition used, **195,088 B** free

## Upstream

- Source: [`xuanyide01/9-nine-rw5`](https://github.com/xuanyide01/9-nine-rw5),
  pinned at `6a8907a5ae576f4377abe4b74f15bd9af13efe07`.
- Framework: `feature/galgame-framework` in this fork.
