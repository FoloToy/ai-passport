<p align="right"><strong>English</strong> · <a href="README.zh_CN.md">简体中文</a></p>

# eden* reader

A landscape 320x240 visual-novel reader for eden* on the AI Passport, part of the `v1.0.0` batch of twelve ports (November 2026). Script, backgrounds, sprites and event CGs ship inside the application partition and read fully offline.

## Scale

- 21 chapters, 8,860 lines and 295 CG groups; the packer's internal counters report 2,178 composed frames (1,594 patch deltas), while the upstream tree currently holds 855 JPEG files and the gallery references 2,341 times (2,076 distinct ids) - the frame counter is a packer metric, not a file count. The Chinese font subset carries 2,681 glyphs.

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

- The device reports a largest contiguous heap block of only 8,704 B, so this port does **not** band: it reuses one static 264x168 canvas and scales a single 320x240 lv_image (scale 366, a 1.43x cover) to fill the panel.
- The image object must sit at the bottom of the z-order (move_to_index 0); otherwise the parent change hides the text and the screen looks like one large black block.
- No new heap allocation above 8 KB is usable here; both banding and whole-frame rescaling were measured and rejected.

## Distribution

- Branch: `feature/port-eden-landscape` (https://github.com/Shinku-Chen/ai-passport/tree/feature/port-eden-landscape)
- Release: `v1.0.0-eden` (https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-eden)
- Application image 7,785,072 B, merged image 7,850,608 B, **93.54%** of the partition used, **538,000 B** free

## Upstream

- Upstream port repository: [xuanyide01/eden-rw5](https://github.com/xuanyide01/eden-rw5)
- Application baseline: [`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) at `feature/galgame-framework`

