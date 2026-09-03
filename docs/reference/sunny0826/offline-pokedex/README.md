<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Offline Pokédex

A fully offline Pokédex that turns the AI Passport into a handheld dex: all
1,025 national-dex Pokémon (generations I–IX) live inside the firmware with
their pixel sprites, stats, flavor text, and cries — no network required.

## Publish information

- **Title**: Offline Pokédex
- **Description**: Turn your AI Passport into a pocket Pokédex. All 1,025
  Pokémon across generations I–IX live inside the device — browse anywhere, no
  network needed. From the pixel-art menu, pick "Pokédex" and flip through
  entries with UP/DOWN: click to move one by one, double-click to jump ten,
  long-press to leap between generations. Press OK to hear each Pokémon's real
  cry. Every entry shows its pixel sprite, type badges, height, weight, and
  flavor text on a retro green dex screen that feels like the real thing. Your
  seen records and the last entry you viewed are saved automatically, even
  across power loss.

## What it does

- **All 1,025 Pokémon built in**: national-dex entries 1..1025 (generations
  I–IX) with base stats, English flavor text, and 48×48 pixel sprites embedded
  in the firmware image; the app never touches Wi-Fi or HTTP.
- **Cry playback**: a single OK press plays the current species' cry, decoded
  from an embedded Opus stream served out of a dedicated resource partition.
- **Seen tracking**: browsing marks species as seen, a footer line shows the
  running seen count, and the last viewed entry is restored on the next launch.
- **Retro dex UI**: a green "electronic dex" screen with mint highlights,
  pre-blended sprites (no white borders), number/name/type chips, height and
  weight, and a flavor-text panel.
- **Idle dim**: the backlight dims after 60 seconds without keys and returns
  with any press.
- **Battery readout**: the live battery percentage sits in the dark header.

## Interaction

From the pixel-art main menu, select the "Pokédex" card to enter; an OK
long-press returns to the menu from any page. Inside the dex:

- **UP/DOWN (click)**: previous/next entry (±1).
- **UP/DOWN (double-click)**: jump ±10 entries.
- **UP/DOWN (long-press)**: jump to the first entry of the previous/next
  generation.
- **OK (click)**: play the current species' cry.

## Source

- Repository: `sunny0826/ai-passport`, branch `feature/ble-artwork`
  (<https://github.com/sunny0826/ai-passport/tree/feature/ble-artwork>).
- Released to the community as project `community-ef164568`; cover recorded as
  `offline-pokedex-cover.png` (PNG, 1152×1536, 3:4) — publish metadata only,
  the image lives with the community publication.
- Data, sprites, and cries from [PokeAPI](https://pokeapi.co/) (CC-BY 4.0),
  embedded by `tools/gen_pokedex_static.py` and `tools/gen_pokedex_cries.py`.
