<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Xiaonuo Jianlu (Voice Notes & Lists)

An AI note-taking list that turns the AI Passport into a "capture anytime,
organize anytime" voice memo card. Hold OK and talk — the hub transcribes and
organizes your words into a categorized note card; your list appears as a
tactile stack of cards that syncs across the card, a web UI, and AI assistants.

## Publish information

- **Title**: 小诺简录 · 随说随记的 AI 清单 / Xiaonuo Jianlu · Talk-to-Note AI Lists
- **Description**: hold the button and talk; your words become neatly titled,
  auto-categorized note cards in a stack you flip through. Works offline
  (voice memos queue on-device and can be played back), syncs everywhere
  through an open-source hub.
- **Cover**: `hero-cover.jpg` (JPEG, 1152×1536); extra images
  `card-recording-cover.jpg`, `card-confirm-cover.jpg` (JPEG, 1152×1536).

## What it does

- **Hold-to-talk capture**: hold OK, speak, release. Audio records to a SPIFFS
  partition and streams to the self-hosted hub; the hub transcribes (ASR) and
  uses LLM function calling to create/organize records (type, title, tags,
  summary). A confirmation page shows the new card.
- **Stacked-card list**: the list is a deck of cards — top card large (type
  badge, title, summary, tags, date), cards behind inset and dimmed. UP/DOWN
  flips with a fly-out transition; OK checks off the top card.
- **Offline mode**: when the hub or Wi-Fi is unreachable, a snapshot of the
  list loads from flash and stays browsable; check-offs are queued
  ("pending sync") and voice recordings sit in the list as a playable
  placeholder card ("voice · unrecognized"). Everything syncs automatically
  when connectivity returns.
- **Provisioning**: BLUFI (EspBlufi app) for Wi-Fi credentials (stored in NVS);
  the hub is auto-discovered on the LAN via mDNS (`_xiaonuo._tcp`) — zero
  manual address setup. Long-press UP to re-provision.
- **Power saving**: screen off after 60 s idle, light sleep after 10 min,
  any-key wake with the wake gesture swallowed so it never triggers an action.
- **Hub**: open-source Express + SQLite service
  (<https://github.com/noah-1106/xiaonuo-jianlu>) with an OpenAI-compatible
  LLM/ASR adapter layer, a web UI, an MCP endpoint for AI assistants, and an
  Agent Skill — one dataset, four entry points.

## Interaction

Three keys drive the app. A top bar shows the title and battery percentage.

- **UP / DOWN**: flip through the card stack (page through long text).
- **OK**: complete the top card / confirm / play a pending voice memo.
- **OK (hold ≥ 0.5 s)**: voice capture; release to send.
- **OK (double)**: manual refresh.
- **UP (hold)**: re-provision (clears saved Wi-Fi and hub info after confirm).

## Engineering notes (reference value)

- **Wi-Fi/BLE separation for provisioning**: Wi-Fi and NimBLE cannot coexist in
  this board's RAM, so provisioning runs BLE-only, stores credentials in NVS,
  and reboots into the Wi-Fi-only path.
- **mDNS behind restrictive routers**: some routers do not forward multicast
  responses to the device, so discovery uses a hand-written mDNS query that
  forces unicast legacy responses (RFC 6762 §8.1); the hub advertises via the
  OS-level responder for full compatibility.
- **TWDT as a work-domain watchdog**: tasks register the watchdog only while
  working and unregister while idling — idle tasks never false-trigger, and
  light-sleep entry/exit toggles registration to avoid wake races.
- **Application CJK font**: the built-in 1187-glyph CJK subset is far too
  small for arbitrary ASR text, so a GB2312-coverage 16 px font (~7 k glyphs)
  is generated with `lv_font_conv` from Noto Sans SC (OFL) and tracked with
  its regeneration command; emoji are silently dropped at the glyph layer.
- **Heap discipline without PSRAM**: audio DMA buffers are pre-allocated early
  at boot while the heap is still plentiful (late allocation was the root
  cause of a recording freeze), HTTP runs in a dedicated task with bounded
  static buffers, and steady-state heap stays around 33 KB.

## Source

- **Firmware**: <https://github.com/noah-1106/ai-passport/tree/feature/xiaonuo-jianlu>,
  entry file `main/jianlu_app.c` (UI `main/jianlu_ui.c`, capture
  `main/jianlu_capture.c`, provisioning `main/jianlu_provision.c`)
- **Hub**: <https://github.com/noah-1106/xiaonuo-jianlu> (Express + SQLite,
  web UI, MCP, Agent Skill)
