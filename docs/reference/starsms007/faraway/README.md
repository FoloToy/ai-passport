<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Faraway

A travel journal for an orange cat, built for the AI Passport's 240×320
portrait screen: send the cat out on a trip of 15 seconds to 12 hours, and it
comes back with a postcard and a keepsake from wherever it landed. Fully
offline — no network setup.

## Publish information

- **Title**: Far Away
- **Description**: Some of us stay home and work. The cat goes instead.

  Pick a trip length from the row along the bottom of the home screen — a
  15-second test run, 5 minutes, 1 hour, 4 hours or 12 hours. The longer you
  send it, the farther it goes. Press ● and off it walks; when the time is up
  it mails back a postcard from wherever it landed, plus a small local
  keepsake.

  There are 24 postcards to collect — from the rapeseed fields of Wuyuan and
  the stone alleys of Xitang all the way to Dunhuang's singing dunes and the
  sand ridges of the Taklamakan. Each finished trip also brings back a
  keepsake: 24 of them in all, from osmanthus cake and a woven bamboo basket
  to a seashell and a worn old coin. Both postcards and keepsakes go into the
  collection book — postcards one at a time, keepsakes laid out over two pages
  of twelve. Anything you haven't found yet is only a grey silhouette with no
  name; it reveals itself, name and all, once you've earned it. Flip through
  and it feels like walking those roads again.

  Can't wait? Hold ● to call the cat home early for a few cans of cat food.
  Short on cans? Play the mini-games in the collection book — catch the falling
  cans, whack-a-rabbit, rock-paper-scissors with the cat, or a memory match.
  Rain, snow, fog and fireflies drift through, and so do a little plane, a hot-air
  balloon and soap bubbles. The cat never stops talking either, at home or on
  the road.

  Fully offline — no network setup needed. Cover and extra images are
  illustrations.

## What it does

- **Trip lengths drive distance**: five departure slots (15-second test run,
  5 minutes, 1 hour, 4 hours, 12 hours) select which destination band the cat
  can reach, so a longer trip unlocks the far postcards.
- **24 destinations, 24 postcards**: hand-drawn 240×320 scenes for each
  destination, kept in a collection book you flip through one card at a time;
  a received postcard can also be set as the home-screen background with an OK
  long-press.
- **24 keepsakes in a two-page album**: a separate collection slot laid out as
  two pages of 3×4. An unearned keepsake shows only as a grey silhouette with
  no name; earning it reveals the art and the name. UP/DOWN turns the pages and
  only moves to the next slot once the pages run out.
- **Four unlockable mini-games**: catch the falling cat-food cans, whack-a-rabbit,
  rock-paper-scissors with the cat, and a memory match. One unlocks for every
  six postcards collected; playing them earns cat food.
- **Cat food as a currency**: holding OK on the road spends a few cans to
  recall the cat early, and the mini-games are how you earn more.
- **Seven weather layers**: rain, snow, fog, fireflies, a small plane, a hot-air
  balloon and soap bubbles drift over the scene on their own schedule — a global
  random draw every 90–240 seconds, each appearance lasting 25 seconds, with the
  particles drawn above the cat.
- **Cat dialogue**: two separate line sets, one for at home and one for on the
  road, shown in a speech bubble.
- **Settings page**: screen brightness, adjusted with UP/DOWN.
- **Persistent progress**: trip count, cat food, owned postcards and owned
  keepsakes survive a power cycle, so the collection builds up across sessions.

Every on-screen element is pixel art quantized to a single shared palette; the
sprite atlas and the five Chinese font sets are committed, so a clone builds
without running the asset pipeline.

## Interaction

Buttons are the three-key layout: UP, DOWN and OK (●), plus a hardware power
key that cuts power directly and is not readable by the firmware.

| Key | Home | On the road | Collection book | Settings |
|---|---|---|---|---|
| UP / DOWN | choose trip length | switch the screen off | turn pages / move between slots | adjust brightness |
| OK (click) | confirm departure | open the collection book | enter / accept | go back |
| OK (long-press) | switch the screen off | recall the cat early (costs cat food) | go back | — |

In the collection book, the cursor moves between the mini-game and settings
slots first and only turns the page at the edge; the same edge-first rule
applies to the keepsake album.

## Source

- Repository: `starsms007/ai-passport`, branch `feature/faraway`
  (<https://github.com/starsms007/ai-passport/tree/feature/faraway>), forked from
  upstream `main` at `1051209`.
- Released as GitHub release
  [`v1.0.0-faraway`](https://github.com/starsms007/ai-passport/releases/tag/v1.0.0-faraway)
  with the merged 8 MB image `FoloToy-AI-Passport-full.bin`.
- Published to the community as project `community-751e8cae`; cover recorded as
  `cover_ai_1152x1536.png` (PNG, 1152×1536, 3:4) — publish metadata only, the
  image lives with the community publication.
- Build requirements: ESP-IDF v5.5.3, target `esp32c3`, **8 MB flash** — the
  partition table places the app at `0x7F0000`, so a 4 MB board will reboot
  forever. `components/bsp/` is unmodified from upstream.
- The paired experience entry
  [`lvgl-pool-budget-without-psram.md`](../lvgl-pool-budget-without-psram.md)
  records the LVGL memory budget this application was built against.
