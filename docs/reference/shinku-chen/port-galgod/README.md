<p align="right"><strong>English</strong> · <a href="README.zh_CN.md">简体中文</a></p>

# Galgame Master reader

A landscape 320x240 visual-novel reader for Galgame Master on the AI Passport, part of the `v1.0.0` batch of twelve ports (November 2026). Script, backgrounds, sprites and event CGs ship inside the application partition and read fully offline.

## Scale

- 18 chapters, 5,457 lines and 119,410 characters, 15 choice sets (including quiz scoring and a bad_end_flag branch), four endings plus an after-story, 55 sprites and 38 backgrounds

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

- This title carries its own reader/UI layer rather than the shared gal framework.
- Lower-band seam: the strip canvas needs pivot(0,0), because scaling around the centre drifts by one to three pixels.
- Same-frame sprites are de-duplicated by content (JPEG pointer plus length, not by id), since the pack gives one character several slots pointing at the same image.
- The panel was tightened to 88 px, body text starts at its top and the page counter moved outside it.

## Distribution

- Branch: `feature/port-galgod-landscape` (https://github.com/Shinku-Chen/ai-passport/tree/feature/port-galgod-landscape)
- Release: `v1.0.0-galgod` (https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-galgod)
- Application image 3,285,824 B, merged image 3,351,360 B, **39.48%** of the partition used, **5,037,248 B** free

## Upstream

- Upstream port repository: [hrk666666/GalGod-MiBand-10](https://github.com/hrk666666/GalGod-MiBand-10); snapshot ebe085d2; art from mcpotato1123/galgod-band v2.0.0
- Application baseline: [`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) at `feature/galgame-framework`

