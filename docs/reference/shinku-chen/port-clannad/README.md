<p align="right"><strong>English</strong> · <a href="README.zh_CN.md">简体中文</a></p>

# CLANNAD reader

A landscape 320x240 visual-novel reader for CLANNAD on the AI Passport, part of the `v1.0.0` batch of twelve ports (November 2026). Script, backgrounds, sprites and event CGs ship inside the application partition and read fully offline.

## Scale

- 212 scenes in 195 chapters, 150,288 script ops across 19 routes, and 174 gallery entries

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

- The dialogue box is flush to the bottom edge (170..240), body text starts at its top and the page counter moved outside it so it never covers text.
- The chapter strip sizes itself with LV_SIZE_CONTENT, so a long chapter name is no longer clipped to a fixed width.
- Sprite baselines are derived from the decoded height, and the face changes with the speaker.
- Only 173,056 B of the partition is free, so any content change starts with a flash budget check.

## Distribution

- Branch: `feature/port-clannad-landscape` (https://github.com/Shinku-Chen/ai-passport/tree/feature/port-clannad-landscape)
- Release: `v1.0.0-clannad` (https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-clannad)
- Application image 8,150,016 B, merged image 8,215,552 B, **97.92%** of the partition used, **173,056 B** free

## Upstream

- Upstream port repository: [xuanyide01/clannad-rw5](https://github.com/xuanyide01/clannad-rw5); snapshot 2c677208
- Application baseline: [`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) at `feature/galgame-framework`

