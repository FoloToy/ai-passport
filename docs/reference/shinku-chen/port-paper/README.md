<p align="right"><strong>English</strong> · <a href="README.zh_CN.md">简体中文</a></p>

# Paper no Mahou Tsukai reader

A landscape 320x240 visual-novel reader for Paper no Mahou Tsukai on the AI Passport, part of the `v1.0.0` batch of twelve ports (November 2026). Script, backgrounds, sprites and event CGs ship inside the application partition and read fully offline.

## Scale

- 47 scene scripts, 56,634 ops and 44,521 lines, four choices, six endings, 415 event CGs in 80 groups and 341 sprites (163 full images plus 178 expression patches)

## Controls

| Key | While reading | In lists and choices |
| --- | --- | --- |
| OK (short) | Advance a page; completes the page if it is still typing | Confirm |
| OK (hold) | Open the menu | Go back one level |
| UP (short) | Advance a page | Move the cursor up |
| UP (hold) | Fast-forward while held | - |
| DOWN (short) | Open history | Move the cursor down |
| DOWN (hold) | Toggle auto-read | - |

## Implementation notes

- tjpgd emits B, G, R; packing those bytes straight into RGB565 swaps red and blue, and the more red a frame contains the more purple it looks, so the canvas write swaps to RGB565 first.
- The CG cover ratio is computed at runtime from the canvas height (max(320/w, 240/h)) instead of a hard-coded constant.
- The panel sits at 152..240 (88 px), body text starts at its top and the page counter lives outside it.

## Distribution

- Branch: `feature/port-paper-landscape` (https://github.com/Shinku-Chen/ai-passport/tree/feature/port-paper-landscape)
- Release: `v1.0.0-paper` (https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-paper)
- Application image 6,289,312 B, merged image 6,354,848 B, **75.56%** of the partition used, **2,033,760 B** free

## Upstream

- Upstream port repository: [xuanyide01/paper-rw5](https://github.com/xuanyide01/paper-rw5)
- Application baseline: [`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) at `feature/galgame-framework`

