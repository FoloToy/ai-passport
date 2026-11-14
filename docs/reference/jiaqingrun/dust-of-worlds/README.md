<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Dust of Worlds — God Simulator

A world that keeps evolving on the AI Passport while the player mostly watches.
Six peoples — and four hybrid peoples born where two of them meet — spread over a
generated archipelago, founding cities, splitting, taking sides and dying out on
their own. The player never commands anyone: divine power is spent on disasters,
blessings and answers to prayers, and the chronicle says what came of it.

## Publish information

- **Title**: Dust of Worlds — God Simulator (published in the community library
  under its Chinese title; recorded in the [Simplified Chinese peer](README.zh_CN.md))
- **Published description**:

  Turn AI Passport into a world that evolves on its own. It begins with a few
  thousand beings across six species — Human, Beast, Avian, Scale, Insect and
  Wood — each thriving, allying and fighting in its own lands. When two peoples
  meet, a hybrid people can be born: Human-Beast, Bird-Scale, Beast-Bird or
  Wood-Insect, inheriting different gifts from both parents. The world will not
  wait: wars, plagues, meteors, coups, famines, volcanoes and wildfires unfold;
  rivers change course, glaciers advance and retreat, lava cools and is reclaimed
  by grass. Peoples migrate, move their capitals and discover ruins of an older
  world; a species can vanish — or evolve again after a long silence. Three rulers
  will leave their names: the absurd KANE drags civilization down with grand
  palaces, purged scholars and wars on every side; Daji the Enchantress appears
  among the Beast tribes and makes enemy courts neglect their duties; the First
  Emperor unites the Human tribes and opens a 3,700-year golden age in which time
  itself slows, and his institutions outlive him. Tribes burn incense and pray,
  found holy sites and send pilgrims; high civilizations raise star gates, sky
  towers and great temples. After twilight the old world ends, wonders become
  ruins, and the next cycle begins.

## What it does

- **Ten peoples**: six base species (Human, Beast, Avian, Scale, Insect, Wood)
  plus four hybrids (Human-Beast, Bird-Scale, Beast-Bird, Wood-Insect). A hybrid
  people is founded the first time two base cultures meet, then propagates on its
  own; traits are inherited from both parents. Hybrids that have not appeared are
  absent from the Species page instead of showing as empty rows.
- **A simulation that runs without input**: growth, migration, drifting nomad
  groups, capital moves, wars, plagues, meteors, coups, famines, volcanoes,
  wildfires, rivers changing course, glaciers advancing and retreating, ruins of
  an older world being discovered, and species going extinct or re-evolving after
  a long silence.
- **Three fixed rulers with their own cards, colours, banners and sounds**:
  KANE issues absurd edicts (palaces, purged scholars, wars on every side, amnesty,
  self-demotion) and waits for tolerate / suppress / silence; tolerating them
  accumulates absurdity that can demote a civilization. Daji works passively from
  the Beast tribes — courts neglect their duties, generals defect — and needs no
  input. The First Emperor unites the Human tribes, opens a 3,700-year golden age
  and slows time while he reigns (10 years per tick instead of 100), then leaves
  institutions behind.
- **Prayers, holy sites and wonders**: tribes burn incense and pray (the world
  pauses and asks for grant / punish / silence, 20 s unanswered counts as
  silence); they found holy sites and send pilgrims; high civilizations raise star
  gates, sky towers and great temples, which can be inspected with a one-second
  zoom shot from the World page.
- **Twilight and rebirth**: after the old world ends, its wonders become broken
  columns, myths and heroes are still sung, stardust falls and the next cycle
  starts; the Data page shows how many cycles have passed.
- **Intervention has a price**: divine power refills from the living's faith;
  disasters accumulate wrath that suppresses the refill, and the world's climate
  anomaly cools or warms every people.
- **Persistence**: manual save / load / restart plus an automatic save every
  180 s; loading a bad save never destroys the existing one.

## Interaction

Three keys only: UP / DOWN step and accelerate, OK executes and pauses, holding
UP / DOWN flips pages, holding OK sleeps.

| Page | UP (short) | DOWN (short) | OK (short) |
| --- | --- | --- | --- |
| World | speed up | slow down | pause / resume |
| Chronicle | older entries | newer entries | pause and open the entry |
| Data | previous action | next action | run the action |
| Species | previous people | next people | open share, groups, settlements, traits |
| Atlas | — | — | back to the World page |

- **Speed** has three steps: paused, 1× (600 ms per tick), 4× (150 ms) and 12×
  (50 ms); one tick is 100 years.
- **Atlas** is page five and a pure display: the map fills the panel, rotated 90°,
  with no banners, starlight, colour layers or hints. Anything that needs an
  answer — a prayer, a KANE edict, a character card, the wake report — first
  returns to the World page and opens there.
- **Sleep** (hold OK) turns the backlight off, saves immediately and lets the
  world keep evolving at no more than 1×; the first key press wakes the screen and
  shows a report of years passed, population change, peoples founded and lost,
  wars and events. A prayer wakes the screen on its own so the world can ask.
  Auto-sleep is on by default after 60 s without input and can be disabled; the
  Data page also sets brightness (40 / 60 / 80 / 100) and volume.

## Source

- Not published as a public Git repository; the application is distributed through
  the community library under project `community-c04b753d` (`projectId 2170`),
  latest published revision `5382` (v0.36), with v0.51 submitted as revision
  `5448` and pending review.
- Built from the AI Passport `main/` layout with the BSP components unchanged;
  pages, simulation and state live in `main/` (`dust_sim.c` for the world model,
  `dust_ui.c` for the five pages, `main.c` for keys, audio, sleep and storage).
- Cover recorded as `cover.webp` (WebP, 900 × 1200, 3:4) — publish metadata only,
  the archive is text-only.

## Build notes

- ESP32-C3, 8 MB flash, no PSRAM, ESP-IDF 5.5.3, LVGL 9; the panel is a
  240 × 320 ST7789P3.
- The merged image is 1,057,344 bytes; the application binary is 926,272 bytes.
- One bitmap font carries all UI text: 1,234 glyphs (95 ASCII plus 1,139 CJK) at
  4 bpp, with a static coverage gate over the application's own strings so a
  missing glyph fails the build instead of showing an empty box.
- After boot the v0.51 build reported about 55 KB of free internal heap and 72 % of
  the LVGL pool in use.
- Two experience entries written from earlier releases of this application cover
  the simulation side and the rendering / audio side: see
  [Keeping a Persistent World Alive on a Three-Key Handheld](../persistent-world-simulation-on-a-handheld.md)
  and [Live Overlays, Redraws and Audio on a No-FPU Handheld](../live-overlays-and-audio-on-a-handheld.md).
