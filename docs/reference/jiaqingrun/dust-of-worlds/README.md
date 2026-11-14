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
  and slows time while he reigns, then leaves institutions behind.
- **Prayers, holy sites and wonders**: tribes burn incense and pray; they found
  holy sites and send pilgrims; high civilizations raise star gates, sky towers
  and great temples, which can be inspected with a one-second zoom shot from the
  World page.
- **Persistence**: manual save / load / restart plus an automatic save every
  180 s; loading a bad save never destroys the existing one.

## How the world works

### Divine power and wrath

Power runs from 0 to 100 and starts at 30. Each tick it regenerates from two
sources — the number of peoples alive and the faith each of them holds — and that
regeneration is scaled by `(1 - wrath * 0.008)`, so a fully wrathful world (100)
regenerates at one fifth of the rate. Wrath decays by 0.004 per tick; every
disaster adds 10 to 16 of it. Above 35 wrath the world can push back at you at
intervals, which is the price of treating the world as a sandbox.

Six interventions, each with a fixed cost:

| Intervention | Cost | Effect |
| --- | --- | --- |
| Star fire | 45 | meteor strike: instant loss, scorched land, a decade of cold |
| Ice age | 60 | global cooling, glaciers advance, peoples migrate south |
| Flood | 34 | destroys a city's stores, leaves fertile silt behind |
| Volcano | 40 | kills now, ash becomes the best farmland centuries later |
| Blessing | 26 | raises the chosen people's fortune |
| Listen | 0 | hears the current prayer without answering it |

Milestones also pay: the first tribe (+10), first city (+12), first kingdom
(+15), first civilization (+25), first temple (+10) and each population threshold
(+15 / +20 / +30 / +40). Watching the world is therefore the main income, not
punishing it.

### Answers and their consequences

- **Grant a prayer**: the people's population ×1.25, faith +20, tech +5 and a
  year of food — the cheapest way to make one people strong, and to raise the
  faith that feeds your power.
- **Punish a prayer**: population ×0.70 and faith −15.
- **Stay silent**: faith −8. Twenty seconds without an answer counts as silence,
  so a player who is watching the Atlas page still pays for ignoring the world.

### Climate

A global temperature anomaly sits between −0.70 and +0.30. Cold converts land to
tundra and pushes peoples toward the equator; warmth melts the ice edge back into
grass. Sky events — comets, eclipses, meteor showers — nudge the anomaly down,
and the Star fire intervention adds a longer cold decade on top.

### Civilization and the ages

Tech sets a people's tier — settlement, city, kingdom, civilization, empire,
divine dynasty — and culture decides how well it absorbs its neighbours. Each new
age is announced in the Chronicle. When an empire-tier people exists and at least
four peoples are still active, the world enters the dusk age, and the Chronicle
starts a countdown.

### Twilight and rebirth

Dusk lasts 18,000 ticks (1.8 million years) before the old world ends; a world
whose peoples are all dead for 2,000 ticks ends early. On the end the wonders
become broken columns, one hero's name is kept as the legacy of that cycle, every
people is gone, and the world starts again with a new continent and a new
chronicle — the Data page counts the cycles.

## Playing a run

1. **Boot and read the map.** The World page shows the six colours plus magenta
   for wonders, white-gold for holy sites and the hybrid mark. OK pauses, so the
   first thing to do is pause and look.
2. **Let the early world alone.** At settlement and city tier your power is low
   and a disaster mostly costs you the faith income you need later. Let the
   milestones fire; they are the earliest large power income.
3. **Read the Chronicle, not the map.** It records cause as well as event (who
   died, what killed them, which people split), and it is where a run's story
   appears. The map alone tells you where; the Chronicle tells you why.
4. **Intervene where the seams are.** A flood that ruins a granary also leaves
   silt; a volcano kills a city and fertilises the valley below it. Every disaster
   is a change of terrain first and a killing second.
5. **Answer prayers on purpose.** Granting builds faith (and therefore power);
   punishing is for a people you want gone; silence is a real option but costs
   faith, and it happens by itself after 20 seconds.
6. **Sleep instead of leaving it on.** Holding OK saves, turns the screen off and
   keeps the world at 1×; the wake report tells you how much you missed. Nothing
   is lost by walking away for an hour.
7. **There is no failure state.** You cannot lose; you can only miss the century
   in which something was worth watching. The dusk countdown exists to make that
   point eventually.

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
- **Double clicks** (not in the table): a double OK from the World page cycles a
  one-second zoom on each wonder, a double UP / DOWN jumps the speed.

## Source

- Repository: `Jiaqingrun/ai-passport-dust-of-worlds`, branch `main`
  (<https://github.com/Jiaqingrun/ai-passport-dust-of-worlds>).
- Published to the community library as project `community-c04b753d`
  (`projectId 2170`); latest published revision `5382` (v0.36), with v0.51
  submitted as revision `5448` and pending review.
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
- The save blob is a versioned (v3) compact binary: 60 bytes per tribe, 27 per
  group, 79 per chronicle entry, plus the terrain and fertility layers; the whole
  world fits in a 30 KB buffer, which is why the automatic save is skipped when
  free heap is too low instead of failing halfway.
- Two experience entries written from earlier releases of this application cover
  the simulation side and the rendering / audio side: see
  [Keeping a Persistent World Alive on a Three-Key Handheld](../persistent-world-simulation-on-a-handheld.md)
  and [Live Overlays, Redraws and Audio on a No-FPU Handheld](../live-overlays-and-audio-on-a-handheld.md).
