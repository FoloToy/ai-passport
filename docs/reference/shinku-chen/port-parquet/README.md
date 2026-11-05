<p align="right"><strong>English</strong> · <a href="README.zh_CN.md">简体中文</a></p>

# PARQUET reader

A landscape 320x240 visual-novel reader for PARQUET on the AI Passport, part of the `v1.0.0` batch of twelve ports (November 2026). Script, backgrounds, sprites and event CGs ship inside the application partition and read fully offline.

## Scale

- 107 scenes in 434 script blocks, 11,205 ops and 11,098 lines, four choices and four endings; 367 event CGs in 37 groups and 979 sprite ids are reference counts from the packer (the `images/p014` and `s015` directories hold 400 JPEG files).

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

- Art uses a two-band canvas (2 x 320x120); both bands are drawn before a single flush, so a full-screen CG no longer needs a whole-frame rescale.
- Chapters continue automatically on the same route only; the last chapter or a route switch lands on the ending page, and the automatic save is written the moment a chapter turns.
- Sprite baselines come from the decoded height (body_dy plus h is always 150), which removes the floating gap above the panel.
- Fast-forward shows the whole page at once and de-duplicates by a visual fingerprint, so an unchanged frame is not decoded again.

## Distribution

- Branch: `feature/port-parquet-landscape` (https://github.com/Shinku-Chen/ai-passport/tree/feature/port-parquet-landscape)
- Release: `v1.0.0-parquet` (https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-parquet)
- Application image 7,181,616 B, merged image 7,247,152 B, **86.29%** of the partition used, **1,141,456 B** free

## Upstream

- Upstream port repository: [xuanyide01/parquet-rw5](https://github.com/xuanyide01/parquet-rw5); snapshot 8ab38afe
- Application baseline: [`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) at `feature/galgame-framework`

