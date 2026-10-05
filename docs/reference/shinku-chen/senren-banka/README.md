<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Senren * Banka

A portrait visual novel reader for the AI Passport, ported from the Mi Band fan
port of *Senren \* Banka*. The whole story, its backgrounds, sprites and event
illustrations live in the firmware, and three keys carry you from the prologue to
the ending.

## Publish information

- **Title**: Senren \* Banka
- **Description**: submitted as:

  > Turn AI Passport into a pocket visual-novel reader: the whole story and cast of Senren * Banka in your hand, readable from beginning to end with just three buttons. Auto-read keeps the pages turning on their own, holding Up fast-forwards, and you can save or resume whenever you like.
  >
  > A taste of the story:
  >
  > In a hot-spring town tucked deep in the mountains, so far from the main roads that even the buses never come, life has kept its own slow rhythm. Over the years the town grew famous for one peculiar local attraction: a sacred blade sealed in stone, never once drawn by anyone who tried.
  >
  > Then a visiting student snaps it in half.
  >
  > The "compensation" demanded of him turns out to be a marriage — to the daughter of the shrine's keeper, whom he has just met. And that is only the beginning: a fiancée he barely knows, a mysterious girl only he can touch, and a curse still sleeping beneath the town.
  >
  > Where their meetings and romances eventually lead, only the hands that hold that blade will know.
  >
  > (Synopsis adapted from the Moegirlpedia article on Senren Banka, CC BY-NC-SA 4.0.)

- **Category**: games
- **Cover**: `senren-banka-cover.png` (PNG, 420 × 560, 3:4) — publish metadata
  only; the cover image is not committed here.
- **Source**: <https://github.com/Shinku-Chen/ai-passport>

## What it does

- **A complete reading app, not a demo**: the firmware boots straight into the
  reader's own title screen. There is no test menu and no network use; the story
  is read entirely offline.
- **The full game script**: every chapter and dialogue line of the source port is
  packed into the firmware, together with 92 backgrounds, 123 character sprites in
  several poses each, 570 event illustrations and the music-room style extras the
  port ships.
- **Portrait, full-screen art**: backgrounds and event illustrations fill a
  240 × 320 portrait screen, sprites are bottom-aligned and drawn over them, and
  the dialogue sits in a translucent band below.
- **Typewriter text with page breaks**: each line types out at a selectable speed;
  a tap completes the line immediately, and a line that does not fit flips to the
  next page instead of being cut.
- **Chapter cards**: each chapter announces itself as a card reading
  `Chapter X-X` (chapter and section), shown over the current scene; any key, or
  the transition timer, continues reading.
- **Choices**: branch points show the source port's options in place of the
  dialogue; the selection is saved as soon as it is confirmed.
- **Skipping and jumping**: the menu can skip to the start of the next chapter,
  and the reader reports when the last chapter has been reached.
- **Save and resume**: one auto-save plus five manual slots. The auto-save is
  written on every chapter change, on every choice, and before the device sleeps,
  so the title screen's "Continue" always returns to where reading stopped.
- **Settings**: text speed (slow / medium / fast), an about screen describing the
  fan-port provenance, and a power-off entry.
- **Idle handling**: after 60 seconds the picture dims, after 3 minutes the screen
  turns off and after 7 minutes the device deep-sleeps; any key wakes it and
  reading resumes in place. Auto-read and fast-forward are exempt, so a story left
  running never dims or sleeps by itself.

## Interaction

Three keys drive the whole app.

Title screen:

- **Up / Down**: move through the menu (start reading, continue, load progress,
  settings).
- **OK**: enter the selected entry.
- **OK (hold)**: power off.

While reading:

- **Up / Down (tap)**: read the next line; completing a line still being typed and
  turning the page both happen here.
- **OK (tap or hold)**: open the menu — continue, save progress, load progress,
  skip chapter, back to title.
- **Up (hold)**: fast-forward; the reader advances on its own until the key is
  released.
- **Down (hold)**: toggle auto-read, which advances at a fixed pace and keeps the
  screen lit.

At a choice:

- **Up / Down**: move the highlight; **OK**: confirm.

Save screens:

- **Up / Down / OK**: pick and open the auto-save or one of the five manual slots.
- **OK (hold)**: delete the selected slot.

Press timing: a tap shorter than 180 ms is ignored as a click, and 500 ms of hold
triggers the long-press action.

## How it is built

- The reader carries its own small inflate implementation rather than relying on
  the ROM decompressor, so the whole output of a block is decoded into one buffer
  instead of needing a 32 KB sliding dictionary.
- Images are stored per device geometry in a chunked palette format (backgrounds
  and event illustrations as 240 × 320 JPEG, sprites by pose), and event
  illustration variants are stored as sparse mask patches against a base picture.
- The story is packed into small blocks with a 3 KB ceiling, which keeps the
  firmware's working buffer at 4 KB — the part has no PSRAM to spare.

## Source

- Repository: `Shinku-Chen/ai-passport`, branch `feature/senren-banka`,
  release `v0.1.0-senren-banka`
  (<https://github.com/Shinku-Chen/ai-passport/tree/feature/senren-banka>).
- Upstream work: [`hrk666666/Senren-Banka-MiBand-10`](https://github.com/hrk666666/Senren-Banka-MiBand-10)
  — the Mi Band fan port this branch derives from. It declares no licence; its
  script and artwork ship with the branch under `assets/senren-source/` (92
  backgrounds, 123 sprites, 570 event illustrations and 112 script chunks, plus a
  `MANIFEST.json` recording the upstream ref and a SHA-256 per file; `tools/senren_fetch_source.py`
  can fetch them again), packed into the firmware, and the upstream project is
  credited as the source. Repacking from that directory is byte-identical to the
  committed packs. The port is shared for personal study and exchange only, and
  the first-run notice inside the app says so.
- Released to the community as project `community-07c775dc` (id 679); the
  submission was pending review when this archive was written.
