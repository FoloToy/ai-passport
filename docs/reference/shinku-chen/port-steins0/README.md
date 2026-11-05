<p align="right"><strong>English</strong> · <a href="README.zh_CN.md">简体中文</a></p>

# Steins;Gate 0 reader

A landscape 320x240 visual-novel reader for Steins;Gate 0 on the AI Passport, part of the `v1.0.0` batch of twelve ports (November 2026). Script, backgrounds, sprites and event CGs ship inside the application partition and read fully offline.

## Scale

- 104 chapter entries, six endings and 106 gallery entries; unlocks live in a 34-byte versioned blob

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

- The upstream is an already-converted Vela scene snapshot, executed scene by scene from a table.
- Global unlocks (chapters, endings, gallery) only ever grow, and a failed write is retried with throttling.
- Long OK is always back on a secondary page, so the reader cannot get stuck in a list.

## Distribution

- Branch: `feature/port-steins0-landscape` (https://github.com/Shinku-Chen/ai-passport/tree/feature/port-steins0-landscape)
- Release: `v1.0.0-steins0` (https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-steins0)
- Application image 7,946,288 B, merged image 8,011,824 B, **95.47%** of the partition used, **376,784 B** free

## Upstream

- Upstream port repository: [xuanyide01/steins0-rw5](https://github.com/xuanyide01/steins0-rw5); snapshot dd19f6ce
- Application baseline: [`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) at `feature/galgame-framework`

