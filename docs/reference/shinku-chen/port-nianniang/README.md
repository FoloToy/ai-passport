<p align="right"><strong>English</strong> · <a href="README.zh_CN.md">简体中文</a></p>

# NianNiang Love Story reader

A portrait 240x320 visual-novel reader for NianNiang Love Story on the AI Passport, part of the `v1.0.0` batch of twelve ports (November 2026). Script, backgrounds, sprites and event CGs ship inside the application partition and read fully offline.

## Scale

- 13 scenes, 1,248 lines, two choices, four endings, 16 backgrounds, 15 event CGs and 30 expression sprites

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

- This title is a 240x320 portrait reader; the branch name still says -landscape for historical reasons.
- The frame is split into four independent 240x80 band buffers, each with its own LVGL descriptor, and flushed once per frame; sharing one buffer makes every band show the last content written.
- The gallery shows a CG with contain (fully visible), while reading keeps cover plus top alignment.
- Sprites stand on the bottom edge (320 - h) instead of being cut off by the dialogue box.

## Distribution

- Branch: `feature/port-nianniang-landscape` (https://github.com/Shinku-Chen/ai-passport/tree/feature/port-nianniang-landscape)
- Release: `v1.0.0-nianniang` (https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-nianniang)
- Application image 1,748,528 B, merged image 1,814,064 B, **21.01%** of the partition used, **6,574,544 B** free

## Upstream

- Upstream port repository: [hrk666666/NianNiang-MiBand-10](https://github.com/hrk666666/NianNiang-MiBand-10); itself derived from hrk666666/Senren-Banka-MiBand-10
- Application baseline: [`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) at `feature/galgame-framework`

