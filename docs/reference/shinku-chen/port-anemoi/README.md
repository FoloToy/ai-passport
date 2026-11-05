<p align="right"><strong>English</strong> · <a href="README.zh_CN.md">简体中文</a></p>

# anemoi reader

A landscape 320x240 visual-novel reader for anemoi on the AI Passport, part of the `v1.0.0` batch of twelve ports (November 2026). Script, backgrounds, sprites and event CGs ship inside the application partition and read fully offline.

## Scale

- 71 of 264 scenes (23 common chapters plus the request board), 764,847 of 3,088,939 dialogue bytes; 30 manual save slots plus an automatic slot

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

- Backgrounds and event CGs keep their 320x180 source geometry and are fitted to the 320x240 panel.
- Sprites are composed from a body layer plus native face layers, so an expression never stores a second full image.
- The request board and chapter list are this work's own screens rather than the shared reader menu.
- This build covers the common route and the requests only; walking into a heroine route shows a volume-boundary page instead of silently skipping chapters.

## Distribution

- Branch: `feature/port-anemoi-landscape` (https://github.com/Shinku-Chen/ai-passport/tree/feature/port-anemoi-landscape)
- Release: `v1.0.0-anemoi` (https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-anemoi)
- Application image 8,240,848 B, merged image 8,306,384 B, **99.01%** of the partition used, **82,224 B** free

## Upstream

- Upstream port repository: [xuanyide01/anemoi-rw5](https://github.com/xuanyide01/anemoi-rw5); snapshot 3e27714c
- Application baseline: [`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) at `feature/galgame-framework`

