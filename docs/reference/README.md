<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Reference

This area holds reference material for AI Passport development that is not a
binding requirement: reusable development experience and archived application
playbooks. These are consulted when developing something new, not enforced as
rules. Reference is organized by contributing developer's GitHub username: under
each `reference/<username>/` folder, experience entries are stored as flat files
and application playbooks as subdirectories.

The engineering rules themselves live under
[`../development/`](../development/README.md); the collaboration conventions under
[`../contribution/`](../contribution/README.md).

## Contributors

### Shinku-Chen

**Experience entries:**

- [Audio Compression Trade-offs on ESP32-C3](shinku-chen/audio-compression-trade-offs.md) — how a voice-playback codec was chosen on limited flash (IMA-ADPCM vs Opus vs MP3), with measured capacity and decoder cost.
- [Post-Release Follow-up for the AI Passport Publishing Flow](shinku-chen/post-release-follow-up.md) — confirm the publish destination, include the data partition in a release, and the consent gates for the post-release tracks.
- [Display Refresh and Deep-sleep on ESP32-C3 (No PSRAM)](shinku-chen/display-refresh-and-deep-sleep.md) — direct panel refresh of a single image rect, RTC-GPIO deep-sleep wakeup, and the LVGL object-type misuse crash signature.

**Application playbooks:**

- [Voice Keychain](shinku-chen/voice-keychain/README.md) — a sound-effects keychain that turns the AI Passport into a pocket audio player.
- [What to Eat Today](shinku-chen/eat-what/README.md) — a button-driven food roulette that turns the AI Passport into a "what should I eat?" spinner.

### PhoenixZHC

**Experience entries:**

- [Network Audio Streaming and Memory Budgeting on AI Passport](phoenixzhc/network-audio-streaming-and-memory.md) — bounded HTTP audio streaming, ES8311/I2S ownership, and joint memory budgeting for decoding, JSON, DMA, and LVGL.
- [SoftAP Provisioning and Resource Budgets on AI Passport](phoenixzhc/softap-provisioning-and-resource-budget.md) — DHCP state, captive-portal compatibility, bounded forms and uploads, and no-PSRAM resource planning.

### Y2Lin

**Experience entries:**

- [Implementing the FAP_SCREENSHOT_V1 Serial Screenshot Protocol](y2lin/serial-screenshot-protocol.md) — install the USB-serial-JTAG driver first, substring-match the command, snapshot into a statically reserved full-screen buffer, chunk payload writes to the tx ring buffer, and mute logs during the binary window.
- [Sound-Meter UI: Smoothing, Anchors, and Stray Blocks](y2lin/meter-ui-smoothing-and-layout.md) — an asymmetric EMA for live readouts, creation-time anchors for mascot animations, the usual suspects behind stray screen blocks, and LVGL pool exhaustion as a white-screen cause.

## Adding an experience entry

Each release may produce **one or more** reusable, post-release learnings; each is
added as its own entry (with the release tag or commit as context). Follow the
repository language rule: keep the default `.md` path in English and the paired
`.zh_CN.md` in Simplified Chinese, aligned in the same change.

An entry is a single `.md` file (with its `.zh_CN.md` peer) stored flat under
`reference/<username>/` and named after the entry's content summary in
lowercase-kebab-case (e.g. `audio-compression-trade-offs.md`), describing the
topic rather than an opaque timestamp. Each entry is routed before submission:
general, upstream-benefiting experience goes to the upstream
`FoloToy/ai-passport` as a PR; fork-specific customization stays in the fork per
[`docs/fork-guide.md`](../fork-guide.md).

## Archiving an application

When an application is published, archive it under `reference/<username>/<app-name>/`
with an AI-generated bilingual functional summary (`README.md` / `.zh_CN.md`) and
optionally a how-to guide. The archive is **text-only** — record the cover image
by file name and format only, and do not store the firmware `.bin`. The `plays-archive`
skill drives the archive and its convention.

## Related

- Repository overview and demo branches: [`../README.md`](../README.md)
