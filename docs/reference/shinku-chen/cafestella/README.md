<p align="right">
  <strong>English</strong> · <a href="README.zh_CN.md">简体中文</a>
</p>

# Café Stella reader

A landscape 320×240 visual-novel reader for *Café Stella and the Reaper's
Butterflies* on the AI Passport. The firmware carries the story, art, audio,
opening video, seven main endings and five after stories offline.

## Scale

- 923 story nodes and 53,727 dialogue frames.
- 69 choice groups / 141 options and seven main endings (bad, normal, Kanna,
  Natsume, Nozomi, Mei, Suzune), plus five after stories and 189 chapter entries.
- 121 CG groups / 1,631 variants; 1,850 source images (1,729 backgrounds and 121
  portrait bases, plus 140 face atlases).

## Controls

| Key | While reading | In lists and choices |
| --- | --- | --- |
| OK / UP / DOWN (short) | Advance immediately; skips typing and auto waits | Move the cursor / confirm |
| OK (hold) | Open the menu | Go back one level |
| UP (hold) | Fast-forward; release stops immediately, choices and endings stop without choosing | - |
| DOWN (hold) | Toggle auto-play | - |

## Implementation notes

- The source 432×243 stage is packed at 224×126 (exact 16:9) and drawn as three
  224×42 RGB565 bands scaled by LVGL to 320×180; there is no full-screen RGB565
  canvas.
- Backgrounds use JPEG q10; portraits and face atlases use q18 plus a 2 bpp alpha
  mask (four levels), with transparent RGB filled from the nearest opaque edge.
- Chinese fonts are 16 px / 20 px 4 bpp subsets covering 3,401 codepoints actually
  present in the pack and UI, with no system fallback.
- Complete audio and video are retained: the title BGM is 256.8 s and the opening
  is 92.995 s of audio plus 95.46 s of video. The opening reuses the three
  224×42 bands and adds no full-frame allocation.

## Distribution

- Branch: `feature/cafestella-20261125`
  (<https://github.com/Shinku-Chen/ai-passport/tree/feature/cafestella-20261125>)
- Release: `v1.0.0-cafestella`
  (<https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-cafestella>)
- Community play: <https://ai-passport.folotoy.cn/plays/community-0c95d859>
- Firmware: `FoloToy-AI-Passport-full.bin`, **8,312,208 B** (merged image)

## Upstream

- Source: [`xuanyide01/cafestella-rw5`](https://github.com/xuanyide01/cafestella-rw5),
  pinned at `90ddb9e020970907b61ad4181f25c65267e6e98b`.
- Framework: `feature/galgame-framework` in this fork.
