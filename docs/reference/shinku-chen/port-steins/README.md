<p align="right"><strong>English</strong> · <a href="README.zh_CN.md">简体中文</a></p>

# Steins;Gate reader

A landscape 320x240 visual-novel reader for Steins;Gate on the AI Passport, part of the `v1.0.0` batch of twelve ports (November 2026). Script, backgrounds, sprites and event CGs ship inside the application partition and read fully offline.

## Scale

- the whole script of upstream steins-rw5 0.3.0; the branch records no verifiable chapter or line count, so none is quoted here

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

- Body text is laid out as four fixed lines; pagination counts font units and leaves one column of slack for hanging punctuation.
- The about page needs a 1024-byte buffer: 128 bytes truncates the nine-line introduction.
- The title screen recolours two labels instead of re-rendering the page, which removed a per-key stall caused by re-reading NVS.

## Distribution

- Branch: `feature/port-steins-landscape` (https://github.com/Shinku-Chen/ai-passport/tree/feature/port-steins-landscape)
- Release: `v1.0.0-steins` (https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-steins)
- Application image 7,975,280 B, merged image 8,171,888 B, **97.35%** of the partition used, **216,720 B** free

## Upstream

- Upstream port repository: [xuanyide01/steins-rw5](https://github.com/xuanyide01/steins-rw5); snapshot 28e5b52f
- Application baseline: [`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) at `feature/galgame-framework`

