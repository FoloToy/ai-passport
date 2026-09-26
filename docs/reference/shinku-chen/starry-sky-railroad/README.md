<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Starry Sky Railroad and Shiro's Journey

A portrait visual-novel reader that ports the Mi Band 9 Pro fan port of the
commercial Japanese novel *Hoshizora Tetsudou to Shiro no Tabi* — together with its
Chinese fan translation — to the AI Passport. The whole story runs offline from
Flash: **39 chapters, 1,260 scenes, 13,787 lines of dialogue, one choice and one
ending**, with nothing to copy onto the device.

## Publish information

- **Title**: Starry Sky Railroad and Shiro's Journey (the community listing uses the
  Chinese title; see this archive's Chinese peer)
- **Description**: Turn the AI Passport into a pocket visual novel: it opens on the
  title screen and you read from the first line to the single ending. All 39
  chapters and 13,787 lines of dialogue live on the device — no network, and
  nothing to copy over. A train cuts through the night sky, racing under the
  stars; a carefree solo journey in the cool night breeze, and along the way a
  laid-back passenger, an endearing conductor… and a girl with cat ears? One
  choice in the middle of the story decides how the journey ends. Only the
  character who is speaking appears on screen — narration and event illustrations
  stay clean — and every scene is saved automatically, so you can close it
  whenever you like and pick up right where you stopped.
- **Category**: games
- **Source submitted at publish time**:
  <https://github.com/Shinku-Chen/ai-passport/tree/feature/starry-sky-railroad>
- **Cover**: `starry-sky-railroad-cover.png`, PNG, 1152 × 1536 (exact 3:4). The
  archive is text-only; the image itself is not stored here.
- **Community project**: `community-0d8223f7` (submitted 2026-09-27).
- **Release**: tag `v0.1.0-starry-sky-railroad` on
  `feature/starry-sky-railroad`, shipping the merged image
  `FoloToy-AI-Passport-full.bin`.

## What it does

- **Boots straight into the reader**: the title screen offers start, continue (the
  automatic save), manual save slots and settings. No demo menu, no network.
- **The ATRI reader's LVGL page system** drives every page: title, body text with a
  typewriter effect, choices, the in-game menu, settings, five manual save slots
  plus one automatic slot, the chapter-transition card, the ending card and about.
- **A sprite appears only while its own character is speaking.** The packer derives
  the owner from which named speaker references a sprite; narration and other
  speakers hide it, and event illustrations or solid-colour scenes never get a face
  pasted on top. In the shipped script 3,444 of 11,777 dialogue steps draw a sprite.
- **Fast-forward stops when the key is released**, and auto-reading advances about
  0.9 s after each page has finished typing (any other key stops it).
- **Always resumable**: progress is written automatically on every scene change.
  Left alone the device dims after 60 s, turns the screen off after 3 min and
  sleeps after 7 min; any key wakes it back into the story.

## Layout

```text
┌────────────────────────────┐  240 x 320, held upright (portrait)
│ [chapter 8]      [battery] │  plain chapter text top-left, battery text top-right
│  art area                  │  one full-screen 240 x 320 background JPEG with the
│  240 x 320                 │  character sprite on it and a speaker name plate over
│  [speaker]                 │  the art, left of the text band
├────────────────────────────┤
│  body text                 │  translucent blue text band over the art, 110 px tall
│                            │  16 px font: 13 full-width chars/line, 5 lines
└────────────────────────────┘
```

The bottom 110 px carry the translucent blue text band, and the sprite is drawn
*under* that band — like the source port, so the character's lower body sits inside
the text box while everything above the band stays untouched. Only the text rows
get a light scrim, which keeps white text readable over any art.

## Interaction

| Key | While reading | In lists |
| --- | --- | --- |
| UP / DOWN (short) | next line (finishes the typewriter first) | move the cursor |
| UP (hold) | fast-forward while held; stops on release | — |
| DOWN (hold) | auto-reading on/off (~0.9 s per page) | — |
| OK (short) | menu: continue, save, load, skip chapter, back to title | select |
| OK (hold) | — | go back |

The menu's *skip chapter* jumps to the start of the next chapter and plays the
chapter-transition card; on the last chapter it reports that the story is already
at the end. In save mode, holding OK on a slot deletes it.

## Source

- Repository and branch: `Shinku-Chen/ai-passport`, `feature/starry-sky-railroad`
  (<https://github.com/Shinku-Chen/ai-passport/tree/feature/starry-sky-railroad>).
- Script, artwork and the Chinese translation come from the public Mi Band fan port
  [`liuyuze61/Starry_Sky_Railroad_and_Shiro-s_Journey_miband9P`](https://github.com/liuyuze61/Starry_Sky_Railroad_and_Shiro-s_Journey_miband9P)
  (Mi Band 9 Pro quick app), converted into a reader pack. The original title is a
  commercial release: this is a personal fan port, so please support the original.

## Known gaps

- Chapter 7 is absent in the source data (the chapter files jump from 6 to 8), and
  the source's branch flags are never set, so the story runs 1 → 40 to the single
  `FIN` ending. The port reproduces the data as published.
- Because there is only one ending and no per-ending unlock flags, the title menu
  has no "true ending" entry.
- Latin runs use the source font's proportional advances while the line model
  budgets half-width cells, so a line mixing a longer English word with CJK can
  overflow the text area by up to one full-width glyph (2 of 19,986 lines in the
  shipped script); the LVGL label wraps within its box.
- No audio: the source port ships none, and this port adds none.
- The whole story line has not been played end to end on hardware; individual
  chapters, page turns, sprite/band order, the chapter-skip flow and the on-device
  screenshot hook were verified on a real device.
