<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Reference

This area holds reference material for AI Passport development that is not a
binding requirement: reusable development experience and archived application
playbooks. These are consulted when developing something new, not enforced as
rules. Reference is organized by contributing developer's GitHub username: under
each repository-relative `docs/reference/<username>/` folder, experience entries
are stored as flat files and application playbooks as subdirectories.

The engineering rules themselves live under
[`../development/`](../development/README.md); the collaboration conventions under
[`../contribution/`](../contribution/README.md).

## Contributors

### Shinku-Chen

**Experience entries:**

- [Audio Compression Trade-offs on ESP32-C3](shinku-chen/audio-compression-trade-offs.md) — how a voice-playback codec was chosen on limited flash (IMA-ADPCM vs Opus vs MP3), with measured capacity and decoder cost.
- [Post-Release Follow-up for the AI Passport Publishing Flow](shinku-chen/post-release-follow-up.md) — confirm the publish destination, include the data partition in a release, and the consent gates for the post-release tracks.
- [Display Refresh and Deep-sleep on ESP32-C3 (No PSRAM)](shinku-chen/display-refresh-and-deep-sleep.md) — direct panel refresh of a single image rect, RTC-GPIO deep-sleep wakeup, and the LVGL object-type misuse crash signature.
- [Shutting Down On-Board Peripherals Before Deep-Sleep](shinku-chen/deep-sleep-peripheral-power-off.md) — verified register shutdown, shared-bus ordering, terminal GPIO states, LCD deep-sleep holds, the `esp_codec_dev_close()` opened-state trap, and remaining hardware loads.
- [Landscape Rotation and a Deep-sleep Key Wake](shinku-chen/landscape-rotation-and-deep-sleep-key-wake.md) — rotating a portrait panel to a 320 × 240 landscape screen through LVGL, why the corner mask must follow the logical resolution, and how an ADC-owned pin makes a low-level deep-sleep wake fire at sleep entry.
- [Idle Power Stages and Wake Guards](shinku-chen/idle-power-stages-and-wake-guards.md) — a key held through a deep-sleep boot being graded a long press and the release-following guard that fixes it, why automatic light sleep stretches countdowns built on `skip_unhandled_events` timers, and what stopping the radio really costs when a protocol keeps retrying.
- [Wall-clock Budgets for On-Device Game AI](shinku-chen/on-device-game-ai-wall-clock-budget.md) — why node-count limits misfire on this board (about 15k nodes per second), iterative deepening against a time budget, yielding to keep the idle task fed, and difficulty as a blunder rate.
- [Size Static Buffers from the Panel, and Verify the Release Artifact](shinku-chen/release-artifact-verification.md) — a 51 KB buffer mistake that left 8 KB of free heap, reading the startup log of the published merged image, and replacing a just-published release instead of shipping a follow-up.
- [Two-Device BLE Link Between AI Passport Boards (No PSRAM)](shinku-chen/two-device-ble-link.md) — symmetric peer discovery with an address tiebreak instead of host/join, measured link heap on a no-PSRAM part and its conflict with a static screenshot buffer, two hardware-only NimBLE GATT traps (missing `access_cb`, `EDONE` after a successful subscribe), NVS for RF calibration, and a stop-and-wait layer for turn-based play.

- [Packing a Visual-Novel Script for a No-PSRAM Board](shinku-chen/vn-script-pack-budget-and-failure-modes.md) — a 5.06 MB script packed into 1.45 MB, why the chunk size is set by the largest free block (7.7 KB, not by the free heap), and three unrelated defects that all presented as "the story ends immediately" plus the boot self-check that named them.

**Application playbooks:**

- [Voice Keychain](shinku-chen/voice-keychain/README.md) — a sound-effects keychain that turns the AI Passport into a pocket audio player.
- [What to Eat Today](shinku-chen/eat-what/README.md) — a button-driven food roulette that turns the AI Passport into a "what should I eat?" spinner.
- [Connect Four](shinku-chen/connect-four/README.md) — a landscape 10 × 7 four-in-a-row game with three computer difficulty levels, a two-player mode, synthesized sound, and an idle deep sleep.
- [Asunabi](shinku-chen/asunabi/README.md) — a portrait visual-novel reader that carries a complete 30-chapter story, originally a Xiaomi Band quick app, with six save slots, chapter select, and auto-play.
- [Saya no Uta](shinku-chen/saya-no-uta/README.md) — a landscape visual-novel reader with 44 chapters, 3,828 dialogue lines and three endings, read fully offline with three keys.
- [ATRI Reader](shinku-chen/atri-reader/README.md) — a portrait visual-novel reader that plays the complete *ATRI -My Dear Moments-* story offline: 34 chapters, 1,069 scenes, 12,188 dialogue lines, speaker-driven full-body sprites and three endings.
- [Starry Sky Railroad and Shiro's Journey](shinku-chen/starry-sky-railroad/README.md) — a portrait visual-novel reader carrying a 39-chapter fan port offline, with per-speaker sprites and an automatic save on every scene.
- [Senren * Banka](shinku-chen/senren-banka/README.md) — a portrait visual novel reader that carries the whole game — story, backgrounds, sprites and event illustrations — on the device, with auto-read, fast-forward, chapter skipping and save slots.
- [Sanoba Witch](shinku-chen/sanoba-witch/README.md) — a portrait visual novel reader with 101 chapters, five routes and five endings, packed entirely into Flash.

### PhoenixZHC

**Experience entries:**

- [Network Audio Streaming and Memory Budgeting on AI Passport](phoenixzhc/network-audio-streaming-and-memory.md) — bounded HTTP audio streaming, ES8311/I2S ownership, and joint memory budgeting for decoding, JSON, DMA, and LVGL.
- [SoftAP Provisioning and Resource Budgets on AI Passport](phoenixzhc/softap-provisioning-and-resource-budget.md) — DHCP state, captive-portal compatibility, bounded forms and uploads, and no-PSRAM resource planning.

### Y2Lin

**Experience entries:**

- [Implementing the FAP_SCREENSHOT_V1 Serial Screenshot Protocol](y2lin/serial-screenshot-protocol.md) — install the USB-serial-JTAG driver first, substring-match the command, snapshot into a statically reserved full-screen buffer, chunk payload writes to the tx ring buffer, and mute logs during the binary window.
- [Sound-Meter UI: Smoothing, Anchors, and Stray Blocks](y2lin/meter-ui-smoothing-and-layout.md) — an asymmetric EMA for live readouts, creation-time anchors for mascot animations, the usual suspects behind stray screen blocks, and LVGL pool exhaustion as a white-screen cause.

### sunny0826

**Application playbooks:**

- [Offline Pokédex](sunny0826/offline-pokedex/README.md) — a fully offline Pokédex that embeds all 1,025 Pokémon, their sprites, and cries in the firmware.

## Adding an experience entry

Each release may produce **one or more** reusable, post-release learnings; each is
added as its own entry (with the release tag or commit as context). Follow the
repository language rule: keep the default `.md` path in English and the paired
`.zh_CN.md` in Simplified Chinese, aligned in the same change.

An entry is a single `.md` file (with its `.zh_CN.md` peer) stored flat under
`docs/reference/<username>/` and named after the entry's content summary in
lowercase-kebab-case (e.g. `audio-compression-trade-offs.md`), describing the
topic rather than an opaque timestamp. Each entry is routed before submission:
general, upstream-benefiting experience goes to the upstream
`FoloToy/ai-passport` as a PR; fork-specific customization stays in the fork per
[`docs/fork-guide.md`](../fork-guide.md).

## Archiving an application

When an application is published, archive it under the repository-relative
`docs/reference/<username>/<app-name>/`
with an AI-generated bilingual functional summary (`README.md` / `.zh_CN.md`) and
optionally a how-to guide. The archive is **text-only** — record the cover image
by file name and format only, and do not store the firmware `.bin`. The `plays-archive`
skill drives the archive and its convention. Add the application to this index
and its Simplified Chinese peer in the same change.

## Related

- Repository overview and demo branches: [`../README.md`](../README.md)
