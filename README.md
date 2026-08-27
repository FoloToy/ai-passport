<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# FoloToy AI Passport — Projects

This fork of [`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport)
hosts several independent applications developed on the AI Passport wearable
(ESP32-C3, 8 MB Flash, no PSRAM). Each project lives on its own `feature/*`
branch and is introduced below. This page is the **catalog of the projects in
this fork**; see the upstream
[`docs/README.md`](docs/README.md) for the board baseline, hardware, and the
development workflow.

## Projects

### Voice Keychain

A sound-effects keychain that turns the AI Passport into a pocket audio player.
Open it and play one of hundreds of Chinese voice clips from dozens of character
packs — jojo, meme cat, Liu Huaqiang, Haji Mi, Nailong, and more. Three keys
drive it: **UP / DOWN** to move in a list, **OK** to enter a directory, select a
clip, or play it, and **OK (hold)** for settings (battery and volume) or to go
back. A top bar shows the battery percentage.

- Branch: [`feature/voice-keychain`](https://github.com/Shinku-Chen/ai-passport/tree/feature/voice-keychain)
- Archive: [`plays/shinku-chen/voice-keychain/`](plays/shinku-chen/voice-keychain/README.md)

### What to Eat Today

A food-roulette decision helper. Hold **UP** to run the "what should we eat for
lunch?" guide animation, hold **DOWN** to spin through the food selector, and
release to stop on a random food. It shows a live battery reading and
auto-poweroffs after a period of idle. The frames are embedded as index-colored
SPIFFS/LVGL assets so the animation runs without a PSRAM buffer.

- Branch: [`feature/cheerful-goodall`](https://github.com/Shinku-Chen/ai-passport/tree/feature/cheerful-goodall)
- Entry: `main/demo_eat_what.c`; cover art at `assets/images/eat-what-cover.png`

### Shengzi Cards

A Chinese-character flashcard memorization app. Three modes:

- **Browse** — scroll through character cards.
- **Self-test** — mark each character as learned / not learned.
- **Spell** — see the pinyin and guess the character.

Hold **UP / DOWN** to switch modes; a short **OK** press reveals the answer in
the self-test / spell modes. The "learned" marks persist to NVS.

- Branch: [`feature/shengzi-cards`](https://github.com/Shinku-Chen/ai-passport/tree/feature/shengzi-cards)
- Entry: `main/demo_shengzi.c`

## Notes

- Each application is a separate `feature/*` branch off the upstream baseline.
- The hardware baseline, BSP, and contribution rules come from the upstream
  repository (`docs/README.md`, `AGENTS.md`, `docs/contribution/`).
- Reusable engineering experience collected from these releases lives under
  [`docs/experiences/shinku-chen/`](docs/experiences/shinku-chen/) (upstream).
