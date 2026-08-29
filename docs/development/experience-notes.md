<p align="right">
  <a href="experience-notes.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Development Experience Notes

This page is the index for reusable development experience captured after each
firmware release — focused on the fork's own `docs/` differences from upstream:
the documents the developer created or changed on this fork. Individual entries
are stored as separate files under [`../experiences/`](../experiences/), grouped
by the contributing developer's username, one file per entry, named after the
entry's content summary in lowercase-kebab-case. The `experience-pr` skill writes
new entries there and links them from the index below.

Each entry is routed before submission: general, upstream-benefiting experience
goes to the upstream `FoloToy/ai-passport` as a PR; fork-specific customization
stays in the fork per [`docs/fork-guide.md`](../fork-guide.md).

Before starting a new development, check here for previously recorded, reusable
experience — alongside [`plays/`](../../plays/README.md) for reference
applications.

## How to add an entry

Each release may produce **one or more** reusable, post-release learnings; each
is added as its own entry (with the release tag or commit as context). Follow the
repository language rule: keep the default `.md` path in English and the paired
`.zh_CN.md` in Simplified Chinese, aligned in the same change.

Each entry is a single `.md` file with its paired `.zh_CN.md`, stored under
`docs/experiences/<username>/` and named after the entry's content summary in
lowercase-kebab-case (for example `audio-compression-trade-offs.md`), so the
filename describes the topic instead of an opaque timestamp. `<username>` is the
contributing developer's GitHub username (lowercase-kebab-case, e.g.
`shinku-chen`), and groups that developer's entries together instead of
flattening them directly under `docs/experiences/`.

A developer is not limited to a single entry. The archive holds **one or more
entries per developer**, each as its own file (with its paired `.zh_CN.md`) under
that developer's folder, and each linked from the index below. Add a new entry
for each reusable, post-release learning rather than folding it into an existing
one, so each record stays a single, self-contained topic.

## Entries

See [`../experiences/`](../experiences/), and its
[`INDEX.md`](../experiences/INDEX.md) for the archived-entry table, for the
stored entries. The index below lists each entry once it is added.

- **Audio Compression Trade-offs on ESP32-C3** (Shinku-Chen) — how a voice-playback codec was chosen on limited flash (IMA-ADPCM vs Opus vs MP3), with measured capacity and decoder cost. See [`../experiences/shinku-chen/audio-compression-trade-offs.md`](../experiences/shinku-chen/audio-compression-trade-offs.md).
- **Post-Release Follow-up for the AI Passport Publishing Flow** (Shinku-Chen) — confirm the publish destination, include the data partition in a release, and the consent gates for the post-release tracks. See [`../experiences/shinku-chen/post-release-follow-up.md`](../experiences/shinku-chen/post-release-follow-up.md).
- **Network Audio Streaming and Memory Budgeting on AI Passport** (Shinku-Chen) — how to stream MP3, drive the on-board ES8311, and budget internal RAM when HTTP, DMA, task stacks, and the LVGL UI all compete for no-PSRAM memory. See [`../experiences/shinku-chen/network-audio-streaming.md`](../experiences/shinku-chen/network-audio-streaming.md).
- **SoftAP Provisioning, DHCP, and Captive Portal on AI Passport** (Shinku-Chen) — how to build the SoftAP + DHCP + HTTP provisioning flow, auto-open the captive portal, and avoid form-buffer overflow. See [`../experiences/shinku-chen/softap-provisioning.md`](../experiences/shinku-chen/softap-provisioning.md).
