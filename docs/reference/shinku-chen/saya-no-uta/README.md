<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Saya no Uta (Community)

A landscape visual-novel reader for the AI Passport. It ports the Mi Band 10 fan
port of *Saya no Uta* to the device: 44 chapters, 3,828 dialogue lines and three
endings, fully offline, read with three keys.

## Publish information

- **Title**: Saya no Uta (Community)
- **Description** (submitted copy):

  > Turn the AI Passport into a pocket visual-novel reader and revisit Saya no Uta: 44 chapters, 3,828 dialogue lines and three endings, fully offline and ready out of the box.
  >
  > Three keys carry the whole story: UP advances, holding UP fast-forwards until you let go, DOWN steps back a page, and OK opens the menu. Jump straight to any chapter to re-read a favourite scene, or switch on auto-play and let the story read itself. The art fills the whole screen and the translucent dialogue panel shows the scene behind it, so you can finish the whole route without reaching for your phone.
  >
  > This build contains the community-safe base assets only; no adult content is included.

- **Instructions** (submitted separately; Chinese is required):

  > First run: no network and no pairing needed. Power on and follow these steps to read the whole story.
  >
  > 1. The content warning screen comes up first. Scroll it with UP / DOWN; once you reach the end the hint turns into "press OK to continue" and OK enters the game. Before that, OK only pages the text down.
  > 2. On the title screen pick New game / Continue / Saves / Settings / Endings / About with UP / DOWN and press OK. Start with New game; later choose Continue to resume the scene you stopped at.
  > 3. While reading: UP advances a segment, holding UP fast-forwards until you release it, DOWN steps back a page, and OK opens the menu. Long lines are split into pages, so DOWN also lets you re-read the previous page.
  > 4. Auto-play: hold DOWN for about a second on the reading screen (a notice appears), and a segment is read every 0.9 seconds; any key stops it, and choices or endings stop it automatically. The screen will not dim or sleep while auto-play runs.
  > 5. Choices and endings: when choices appear, pick one with UP / DOWN and confirm with OK; the story branches and has three endings in total.
  > 6. Skipping chapters: open the menu with OK and choose Skip chapter. If the chapter still has choices you have not seen, it stops at one of them first.
  > 7. Saves: the Saves screen has five manual slots plus one automatic slot written on every scene change. Holding OK on a slot deletes that save.
  > 8. Settings: the menu's Settings entry switches text speed (including instant) and font size, and opens a scrollable About page.
  > 9. Battery and power: the battery percentage sits in the top-right corner. After about 60 seconds idle the screen dims, at 180 seconds it turns off, and at 420 seconds the device sleeps; any key wakes it.

- **Submission record**: project 669, revision 1410, slug `community-59555253`, status
  `pending` (submitted for review).
- **Category**: games
- **Cover**: `saya-no-uta-cover.png` (PNG, 1152 × 1536, 3:4) — this archive stays text-only and
  records the cover by file name and format only; it is not committed here. The release notes and
  the community listing use the copy kept at `assets/images/saya-no-uta-cover.png` in the
  author's fork.
- **Source**: <https://github.com/Shinku-Chen/ai-passport>

## Cover

`saya-no-uta-cover.png` (PNG, 1152 × 1536) uses the original key visual: Saya with
the title lettering. The cover is recorded here as publish metadata only and is not
committed to the repository.

## Features

- **Three keys, 44 chapters**: OK opens the menu, UP advances (hold to fast-forward,
  release to stop) and DOWN steps back a page; list cursors stop at the ends instead
  of wrapping around.
- **Paginated body text**: 19 full-width characters per line and four lines per
  screen at 16 px (15 characters and three lines at 20 px), with a typewriter effect
  that can also print a whole page instantly.
- **Choices and branches**: one choice in chapter 10 and one in chapter 20, three
  endings in total (End / BadEnd / MadEnd); a choice always stops and waits.
- **Skip chapter**: the menu entry skips the current chapter, but stops at a choice
  the chapter has not reached yet instead of deciding for the player.
- **Auto-play**: hold DOWN on the reading screen to start; it advances one segment
  every 900 ms once the current segment has finished typing. Any key stops it, choices and endings
  stop it automatically, and the idle screen-off policy is suspended while it runs.
- **Saves**: five manual slots plus one automatic slot written on every scene change;
  "Continue" restores the automatic one. Holding OK on a slot in save mode deletes it.
- **Settings**: text speed, font size, and a scrollable about page.
- **Content warning screen**: the first screen after boot; it must be scrolled to the
  end before OK continues.
- **Offline resource pack**: script, backgrounds and sprites are read straight out of
  Flash with no runtime decompression; decoding happens only when the background or
  sprite actually changes, and sprites are composited through a 1bpp mask.
- **Full-frame art behind the panel**: each background is stored as two 1:1 JPEGs
  (rows 0–149 and 150–239 of one 320 × 240 frame) drawn on two stacked canvases, so the
  whole scene stays visible and the translucent dialogue box (50 % opacity, flush with
  the bottom edge) shows real art instead of a stretched or synthesized fallback.
- **Chinese font subset**: a Noto Sans SC subset (4 bpp, uncompressed) built into the
  firmware, covering both the UI strings and every character in the script.
- **Battery and power**: battery percentage in the top-right corner; idle 60 s dims,
  180 s turns the screen off and 420 s enters deep sleep, with any key waking it.
