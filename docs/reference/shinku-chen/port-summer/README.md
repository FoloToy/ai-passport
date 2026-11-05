<p align="right"><strong>English</strong> · <a href="README.zh_CN.md">简体中文</a></p>

# Summer Pockets REFLECTION BLUE reader

A landscape 320x240 visual-novel reader for Summer Pockets REFLECTION BLUE on the AI Passport, part of the `v1.0.0` batch of twelve ports (November 2026). Script, backgrounds, sprites and event CGs ship inside the application partition and read fully offline.

## Scale

- 422 scenes, 81,040 lines, 188 choice sets, 27 map stages, 14 routes with 252 chapter entries, 136 CG groups (712 variants) and 292 sprites

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

- The dialogue box was tightened to 176..240 (64 px) with three body lines, and the page counter plus hints moved outside it to give the art more room.
- The speaker plate sizes to its content instead of a fixed width.
- The dim threshold is 90 s here (60 s elsewhere) because the opening chapters leave a page on screen for longer.

## Distribution

- Branch: `feature/port-summer-landscape` (https://github.com/Shinku-Chen/ai-passport/tree/feature/port-summer-landscape)
- Release: `v1.0.0-summer` (https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-summer)
- Application image 7,585,456 B, merged image 7,716,528 B, **91.86%** of the partition used, **672,080 B** free

## Upstream

- Upstream port repository: [xuanyide01/summer-rw5](https://github.com/xuanyide01/summer-rw5); snapshot 28f8a41c, converted from the Xiaomi Vela project
- Application baseline: [`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) at `feature/galgame-framework`

