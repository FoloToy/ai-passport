<p align="right">
  <strong>English</strong> · <a href="README.zh_CN.md">简体中文</a>
</p>

# Shinuyakata reader

A portrait 240×320 visual-novel reader for *Shinuyakata* on the AI Passport.
The firmware carries the story, sprites, CG variants and choices offline; it is a
silent reader because the source dataset has no voice, BGM or opening video.

## Scale

- 198 runtime pages and 17,409 text records.
- 11 choice groups and 56 CG groups / 311 CG variants.
- Three manual save slots plus automatic progress; an 80 px dialogue panel with
  three lines at either font tier.

## Controls

| Key | While reading | In lists and choices |
| --- | --- | --- |
| OK / UP / DOWN (short) | Complete the typing animation, then advance a page | Move the cursor / confirm |
| OK (hold) | Open the menu | Go back one level |
| UP (hold) | Immediate fast-forward across chapters; release, choices and endings stop it | - |
| DOWN (hold) | Toggle auto-read; the screen stays awake, including at choices | - |

The menu offers three manual save slots, load, history, font size, text speed and
dialogue transparency. Continue uses the automatic progress record. The gallery
contains unlocked CGs, recalls and chapter entries.

## Implementation notes

- The source dataset supplies no voice, BGM or OP video, so this is a silent
  reader. Unresolved or unreachable source nodes are retained rather than
  replaced with invented content; completeness of the commercial original is not
  independently guaranteed.
- Speaker badges fit their text, and the dialogue panel keeps three lines at both
  font tiers.
- The portrait build, the landscape experiment and the cache-repair work are
  recorded in [`docs/assets/shinuyakata.md`](https://github.com/Shinku-Chen/ai-passport/blob/feature/port-shinuyakata/docs/assets/shinuyakata.md).

## Distribution

- Branch: `feature/port-shinuyakata`
  (<https://github.com/Shinku-Chen/ai-passport/tree/feature/port-shinuyakata>)
- Release: `v0.1.3-shinuyakata`
  (<https://github.com/Shinku-Chen/ai-passport/releases/tag/v0.1.3-shinuyakata>)
- Community play: <https://ai-passport.folotoy.cn/plays/galgame-4>
- Firmware: `FoloToy-AI-Passport-full.bin`, **8,180,560 B** (merged image)

## Upstream

- Source: [`xuanyide01/shinuyakata-rw5`](https://github.com/xuanyide01/shinuyakata-rw5)
- Framework: `feature/galgame-framework` in this fork.
