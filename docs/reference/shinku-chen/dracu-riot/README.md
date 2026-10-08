<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# DRACU-RIOT! Reader

A portrait visual-novel reader that puts the whole MiBand port of *DRACU-RIOT!*
on the AI Passport: five heroine routes with their after-stories and endings,
fully offline, driven by the three buttons. It is the fourth fork application
built on the ATRI / Senren reader page system, with a new data layer for a game
whose source is a linear page table rather than scene files.

## Publish information

- **Title**: `DRACU-RIOT! GalGame` (Simplified Chinese and English).
- **Description**: Turn the AI Passport into a fully offline, portrait visual-novel
  reader for *DRACU-RIOT!*: all five heroine routes, their after-stories and their
  endings ship inside the device, so you can start at the prologue and read
  straight to an ending. Story hook: on a huge artificial island floating at sea,
  the Maritime City is a special zone where gambling and adult entertainment are
  legal business; the protagonist is invited there by a friend, gets caught up in
  a kidnapping and wakes up as a vampire — and the island turns out to be the only
  place the government lets humans and vampires live together. Volume: 52,787
  script pages, about 1.14 million Chinese characters, 54 choice points, 62
  chapter entries, 244 event CGs and 85 backgrounds, all offline. The published
  Chinese description is the same text in Simplified Chinese (see the Chinese peer
  of this page).
- **Source**: `https://github.com/Shinku-Chen/ai-passport/tree/feature/dracu-riot`
- **Cover**: `dracu-riot-cover.png`, PNG, 480 × 640 (3:4), portrait key visual of the
  five heroines. Recorded as publish metadata only; not committed here.

## What it does

- **Boots into the title screen**: starting a new game reads from the prologue
  through to an ending without leaving the reader; a chapter list is a separate
  entry, not the way to start.
- **Five routes, five endings, one normal ending**: choice history drives route
  splits, each route has its own chapters, its own ending and an after-story, and
  the "chapter jump" list reaches all 62 chapter entries including the
  after-stories.
- **Reading controls**: a short press of Up/Down advances one line (the first
  press completes a line that is still typing; long lines paginate), holding Up
  fast-forwards and stops on release, holding Down toggles auto-read, and OK opens
  the menu.
- **Saves**: one auto save (written on chapter changes, on choices and before
  sleep) plus five manual slots; the title screen's "continue" resumes from the
  auto save.
- **Idle policy**: dims in 60 s, screen off in 3 min, deep sleep in 7 min, any key
  wakes it; auto-read and fast-forward never count as idle, so the screen stays
  on while the app is advancing the story by itself.
- **Fully offline**: no network, no pairing, no account; script and art live in an
  embedded pack.

## Interaction and flow

Three buttons: Up, Down, OK.

- **Title screen**: Up/Down move the cursor, OK enters. Menu: start reading /
  continue / load / chapter jump / settings.
- **Reading**: as above; OK opens the menu (resume, save, load, skip chapter, back
  to title).
- **Choices**: Up/Down pick, OK confirms and the choice is saved immediately.
- **Chapter jump**: Up/Down move through the 62 entries, OK jumps, holding OK goes
  back.
- **Saves**: on the save page a short OK saves/loads, a long OK deletes that slot.
- **Endings**: each heroine route ends on its own ending screen; finishing clears
  the auto save so "continue" no longer points at a finished run.

## Publishing metadata (bilingual copy as submitted)

The community submission used the same bilingual title above, category `games`,
no tags, and the source address listed under "Publish information". The cover is
a portrait key visual; two extra gameplay screenshots (title screen and a choice
screen) were submitted alongside it.

## Source and credits

Script and art come from the MiBand port
[`hezdaaa/dracu-riot-miband`](https://github.com/hezdaaa/dracu-riot-miband).
*DRACU-RIOT!* is copyright Yuzusoft; the Simplified Chinese text belongs to its
translation group. The port is an unofficial fan work for personal learning and
technical exchange.

See [Linear Page Table Ports](../linear-page-table-ports.md) for the data
pipeline this application uses, and
[Shared Block Buffers Need One Owner](../shared-block-buffer-caches.md) for the
reader defect the port had to fix.
