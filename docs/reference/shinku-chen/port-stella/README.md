<p align="right"><strong>English</strong> · <a href="README.zh_CN.md">简体中文</a></p>

# Stella of the End reader

A landscape 320x240 visual-novel reader for Stella of the End on the AI Passport, part of the `v1.0.0` batch of twelve ports (November 2026). The script, backgrounds, sprites and event CGs of the **21-chapter main story plus nine after-story chapters** (10,975 lines in total) ship inside the application partition and read fully offline.

> After-story entry: the title screen's after-story item, unlocked by clearing the main story; it starts at `story/101`. The pack carries all thirty scenes (1-21 and 101-109).

## Scale

- 21 chapters, 9,425 lines and one ending (chapter 21)

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

- This is the tightest partition of the batch: only 66,992 B free (99.20%), so any content change starts with a flash budget check.
- The speaker plate uses LV_LABEL_LONG_MODE_DOTS: LVGL 9's LV_LABEL_LONG_DOT means multi-line plus ellipsis and wraps, while CLIP with LV_SIZE_CONTENT computes a width of zero.
- Row highlighting colours only the row label (fixed 304 px width) instead of the label and the value column, which removes the vertical line formed by the gap between them.
- The panel was tightened to 156..240 (84 px), body text starts at its top and the page counter moved outside it.

## Distribution

- Branch: `feature/port-stella-landscape` (https://github.com/Shinku-Chen/ai-passport/tree/feature/port-stella-landscape)
- Release: `v1.0.0-stella` (https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-stella)
- Application image 8,256,080 B, merged image 8,321,616 B, **99.20%** of the partition used, **66,992 B** free

## Upstream

- Upstream port repository: [xuanyide01/stella-rw5](https://github.com/xuanyide01/stella-rw5); snapshot 4fa1e7b5
- Application baseline: [`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) at `feature/galgame-framework`

