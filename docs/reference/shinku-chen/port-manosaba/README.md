<p align="right"><strong>English</strong> · <a href="README.zh_CN.md">简体中文</a></p>

# Mahou Shojo no Majo Saiban reader

A landscape 320x240 visual-novel reader for Mahou Shojo no Majo Saiban on the AI Passport, part of the `v1.0.0` batch of twelve ports (November 2026). Script, backgrounds, sprites and event CGs ship inside the application partition and read fully offline.

## Scale

- 458 source script files, 424 scenes and 90,243 commands, with 559 images (62 backgrounds, 172 CGs, 151 sprites and more)

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

- The pack carries source-engine commands rather than a dialogue stream: a C virtual machine executes them with engine.js / scene.js semantics, including call/return, offer/choice, trial damage and handbook versions.
- This title carries its own reader/UI layer (it does not reuse the shared gal framework), with a witch handbook page and an ending page.
- Thirty manual slots plus an independent automatic slot; a failed write leaves the old save untouched.

## Distribution

- Branch: `feature/port-manosaba-landscape` (https://github.com/Shinku-Chen/ai-passport/tree/feature/port-manosaba-landscape)
- Release: `v1.0.0-manosaba` (https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-manosaba)
- Application image 7,763,280 B, merged image 7,959,888 B, **94.77%** of the partition used, **428,720 B** free

## Upstream

- Upstream port repository: [xuanyide01/manosaba-rw5](https://github.com/xuanyide01/manosaba-rw5); snapshot a309a361
- Application baseline: [`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) at `feature/galgame-framework`

