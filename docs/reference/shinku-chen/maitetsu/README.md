<p align="right">
  <strong>English</strong> · <a href="README.zh_CN.md">简体中文</a>
</p>

# Maitetsu · Niiroku Route reader

A landscape 320×240 visual-novel reader for the Niiroku route of *Maitetsu* on
the AI Passport. The firmware carries the four Niiroku chapters, 35 CGs and the
complete 283.88-second title theme offline.

## Scale

- Four chapters, 1,537 paragraphs and 675 scenes.
- 35 CGs in six groups; the story is linear on this route (one ending).
- 19-screen GalGame framework: title, dialogue, menu, three manual saves,
  automatic progress, history, settings and a gallery with CG / recall / chapter /
  music entries.

## Controls

| Key | While reading | In lists and menus |
| --- | --- | --- |
| OK / UP / DOWN (short) | Advance; typing completes first, long text advances one page | Move / confirm |
| OK (hold) | Open the menu | Go back |
| UP (hold) | Jump to the next sentence immediately; repeat while held, stop on release | - |
| DOWN (hold) | Toggle auto-play | - |

## Implementation notes

- Scenes keep the source composition: background plus body/expression patches
  composited at build time, shown at an exact 16:9 320×180 and never cropped.
- The CG browser pages through all 35 CGs, including indices 33–35 that exceeded
  the framework's former 32-bit CG limit.
- The title theme is the complete 283.88 s, 4 kHz mono source as 4-bit IMA ADPCM
  (586 KB), interpolated to 16 kHz on playback.
- Chapter browsing and CG recall are previews: they never award a permanent
  ending. Only a New Game run that reads all four chapters records completion.

## Distribution

- Branch: `feature/maitetsu-20261125`
  (<https://github.com/Shinku-Chen/ai-passport/tree/feature/maitetsu-20261125>)
- Release: `v1.0.0-maitetsu`
  (<https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-maitetsu>)
- Community play: <https://ai-passport.folotoy.cn/plays/community-386921bf>
- Firmware: `FoloToy-AI-Passport-full.bin`, **7,825,488 B**, **94.02%** of the
  partition used, **497,584 B** free

## Upstream

- Source: [`xuanyide01/maitetsu-rw5`](https://github.com/xuanyide01/maitetsu-rw5),
  pinned at `9c008406853affe973ceb80f62128696054365e7`.
- Framework: `feature/galgame-framework` in this fork.
