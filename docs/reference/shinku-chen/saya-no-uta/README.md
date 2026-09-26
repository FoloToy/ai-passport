<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Saya no Uta (Community)

A landscape visual-novel reader for the AI Passport. It ports the Mi Band 10 fan
port of *Saya no Uta* to the device: 44 chapters, 3,828 dialogue lines and three
endings, fully offline, read with three keys.

## Story

Medical student Fuminori Sakisaka survives a traffic accident that kills his parents, but the
brain surgery that saves him leaves the world around him changed: he sees organs and rotting
flesh everywhere, people as moving lumps of meat, speech as animal noise, and even ordinary
food as revolting.

In his despair the only normal thing left is a mysterious girl, Saya, who came to the hospital
looking for her father. After meeting her again and again, Fuminori falls for her, decides she
is the reason he can bear living in this diseased world, and invites her to live with him.

That is only the door to the madness still to come. The story is a pure, if very different,
love story between Fuminori and Saya.

## Publish information

- **Title**: Saya no Uta (Community)
- **Description** (submitted copy):

  > Story:
  > Medical student Fuminori Sakisaka survives a traffic accident that kills his parents, but the brain surgery that saves him leaves the world around him changed: he sees organs and rotting flesh everywhere, people as moving lumps of meat, speech as animal noise, and even ordinary food as revolting.
  >
  > In his despair the only normal thing left is a mysterious girl, Saya, who came to the hospital looking for her father. After meeting her again and again, Fuminori falls for her, decides she is the reason he can bear living in this diseased world, and invites her to live with him.
  >
  > That is only the door to the madness still to come. The story is a pure, if very different, love story between Fuminori and Saya.
  >
  > What it turns your device into:
  > Turn the AI Passport into a pocket visual-novel reader and revisit Saya no Uta: 44 chapters, 3,828 dialogue lines and three endings, fully offline and ready out of the box.
  >
  > Three keys carry the whole story: UP advances, holding UP fast-forwards until you let go, DOWN steps back a page, and OK opens the menu. Jump straight to any chapter to re-read a favourite scene, or switch on auto-play and let the story read itself. The art fills the whole screen and the translucent dialogue panel shows the scene behind it, so you can finish the whole route without reaching for your phone.
  >
  > This build contains the community-safe base assets only; no adult content is included. The story is graphic in places, so please read with care. It is a personal study port built from a public Mi Band port and is meant for personal devices; please support the official release if you can.

- **Instructions** (submitted separately; Chinese is required):

  > First run: no network and no pairing needed. Power on and follow these steps to read the whole story.
  >
  > 1. The content warning screen comes up first. Scroll it with UP / DOWN; once you reach the end the hint turns into "press OK to continue" and OK enters the game. Before that, OK only pages the text down.
  > 2. On the title screen pick with UP / DOWN and press OK: Continue (only shown when an automatic save exists - it resumes the scene you stopped at), Start from chapter 1, Load save, Settings.
  > 3. While reading: UP advances a segment, holding UP fast-forwards until you release it, DOWN steps back a page, and OK opens the menu. Long lines are split into pages, so DOWN also lets you re-read the previous page.
  > 4. Auto-play: hold DOWN for about a second on the reading screen (a notice appears). Once the current segment has finished typing, a new segment is read every 0.9 seconds. Any key stops it, and choices or endings stop it automatically. The screen will not dim or sleep while auto-play runs.
  > 5. Choices and endings: when choices appear, pick one with UP / DOWN and confirm with OK; the story branches and has three endings in total.
  > 6. Menu (tap OK while reading): Save writes to a manual slot or the automatic slot, Load reads a save, Skip chapter jumps to the next chapter (it stops first at a choice the chapter has not reached yet), Back to title returns to the title screen, and Close menu resumes reading.
  > 7. Saves: five manual slots plus one automatic slot written on every scene change. Holding OK on a slot in the save screen deletes that save.
  > 8. Settings (entered from the title screen): Text speed switches between slow / medium / fast / instant (instant shows the whole page at once), Font size switches 16 px and 20 px, About is a scrollable page, and Back returns to the title screen.
  > 9. Battery and power: the battery percentage sits in the top-right corner. After a long idle period the screen dims (about 60 s), turns off (about 180 s) and the device sleeps (about 420 s); any key wakes it and resumes from the automatic save.

- **Submission record**: project 672, revision 1413, slug `community-10803507`, status
  `pending` (submitted for review). Two earlier submissions of the same build disappeared from
  the creator centre shortly after submitting, so this is the third attempt.
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
