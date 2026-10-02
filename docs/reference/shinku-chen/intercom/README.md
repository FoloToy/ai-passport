<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Pocket Intercom

A phone-tethered AI intercom for the AI Passport: hold **OK**, speak, and a
companion Android app carries your words to an AI assistant on your network and
brings the answer back to the device screen and to the phone. The device never
joins Wi-Fi, never speaks HTTP and never stores a gateway secret — the phone is
the middleman, and audio crosses the Bluetooth link as Opus at roughly 3 KB/s.

## Publish information

- **Title**: Pocket Intercom
- **Description**: submitted as:

  > Turn AI Passport into a pocket intercom: hold OK, talk to it, and the companion app on your phone carries your words to your own AI assistant and brings the answer back to the little screen and the phone.
  >
  > The phone app is the middleman between the device and your backend AI: that can be OpenClaw, an OpenAI-compatible Hermes endpoint, or any other OpenAI-compatible API - switch between them in the app. As long as your phone can reach it, the device can use it. The device itself never joins your network and needs no Wi-Fi setup.
  >
  > How it plays:
  > - Hold OK to talk. The screen turns red while it is getting ready and green when you can speak; let go to send, and the reply lands on the device screen.
  > - Press again to keep going, back and forth like a walkie-talkie. The same conversation shows up in the phone app so you can look back later.
  > - Three buttons, nothing to learn: hold OK to talk, a short OK press only lights the screen, a long UP press opens the device settings (brightness / volume / mic gain / device info / re-pair), and UP / DOWN scroll the history.
  > - Long answers are fine: the device screen scrolls eight lines at a time, a new message starts at its newest line, and anything longer is cut with a "……" marker - UP / DOWN let you read back.
  > - Replies are read out loud by default (turn "device speech" off in Settings - Chat settings if you would rather only read): the phone synthesizes the voice and pushes it to the device, so the device still needs no network.
  > - Great whenever grabbing a phone is awkward: ask about the pot while cooking, check the weather before heading out, or answer a kid's random question. Grandparents and children pick it up instantly.
  >
  > Getting started takes three steps: flash this project's firmware, install the companion app and pair it with the device, then point the app at your AI assistant. With OpenClaw, the very first connection also needs the device approved once on the admin side - the app and the device screen tell you exactly what to do.
  >
  > Version: this release matches firmware **v1.13-intercom** and app **1.13**.
  > **Please keep the phone app updated to 1.13 or newer** - firmware and app ship as a matched pair and only the major version has to match; an older app against newer firmware may fail to connect, show wrong state or miss features.

- **Category**: social
- **Tags**: `family`
- **Cover**: `cover-final-tight.png` (PNG, 1152 × 1536, 3:4) — the app icon; publish metadata only, the image is not committed here.
- **Firmware**: `FoloToy-AI-Passport-full.bin`, 2,340,000 bytes,
  sha256 `a9cc59ad4cf35c5e7e8c72f57ed446123070c7e5fac4122731801e0595731daa`
  (release [`v1.13-intercom`](https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.13-intercom))
- **Community play**: <https://ai-passport.folotoy.cn/plays/799/>
- **Source (firmware)**: <https://github.com/Shinku-Chen/ai-passport/tree/feature/openclaw-intercom>
- **Source (phone app, separate repository)**: <https://github.com/Shinku-Chen/ai-passport-openclaw-android>

## What it does

- **Hold OK to talk**: the device turns red the instant the key goes down and
  green once the gateway is ready; releasing OK sends the recorded turn. Measured
  press-to-green is about 280–290 ms because the recognition channel is kept
  pre-warmed.
- **Reply on the device and in the app**: the recognized text and the assistant's
  answer both appear on the device screen; the same conversation is shown in the
  phone app, which is also where the history is kept.
- **Three buttons**: hold OK to talk, a short OK press only lights the screen, a
  long UP press opens the settings screen (brightness, volume, microphone gain, device
  information, re-pair), and
  UP / DOWN scroll the conversation history.
- **Gateway abstraction on the phone**: OpenClaw over WebSocket RPC with an
  ed25519 device identity, an OpenAI-compatible HTTP endpoint (Hermes and custom
  endpoints, including a full request path), or an Echo gateway for link-only
  testing. Each type keeps its own saved configuration.
- **Save-time validation**: the phone app really connects once before saving and
  reports a readable reason on failure, so a wrong token or path never ends up
  stored as a working configuration.
- **Gateway approval loop**: OpenClaw requires each device to be approved once;
  the app shows `Waiting for gateway approval … (deviceId …)` and retries every
  5 seconds for up to 180 seconds once the approval is granted.
- **Device speech (TTS)**: replies can be synthesized by the phone and played back on the device (**on by default**). The device still needs no network; older firmware without the capability falls back to reading aloud on the phone.
  text; the app does not read them aloud and the settings screen has no related
  switch.
- **Idle behaviour**: the backlight turns off after a minute of inactivity and any
  key restores it; the Bluetooth link stays connected so the next press is
  immediately usable.

## Interaction

- **Top of the screen**: two status lines — `device` and `gateway` — plus battery
  percentage. Both lines read ready when the link and the gateway are usable.
- **Conversation area**: one bubble per turn, newest at the bottom. Long replies
  scroll automatically and stay readable; UP / DOWN page through the history.
- **While recording**: the whole screen turns red (getting ready) and then green
  (you can speak). The screen returns to the normal view once the turn is sent.
- **Settings screen** (long UP): backlight brightness and a device information
  page; short OK or a timeout returns to the conversation.
- **Phone app**: four tabs — chat, overview, device and settings. The device tab
  pairs and shows the connected device; the settings tab holds the gateway type
  and its fields.

## Data and toolchain

- **Wire protocol**: `[A5 5A][TYPE][FLAGS][LEN:2B big-endian][payload]` with a
  magic re-sync rule; types are PCM `0x01`, text `0x02`, control `0x03`, event
  `0x04`, Opus `0x05`. The authoritative document lives in this repository at
  `docs/development/engineering/intercom-wire-protocol.md` (English) and
  `.zh_CN.md`.
- **Audio uplink**: Opus at 16 kHz mono, 60 ms frames (960 samples), complexity 0
  and DTX, about 3 KB/s — roughly a tenth of raw PCM. Encoding runs on static
  task stacks, and a PCM path remains as a fallback frame type.
- **Bluetooth**: Nordic UART Service characteristics, LE Secure Connections with
  a 6-digit passkey shown on the device, and a link that meters frames so the
  phone is never flooded.
- **Phone side**: Kotlin Android app; speech recognition through the same cloud
  channel the device vendor uses, and the gateway adapters described above. No
  secret is ever sent to the device, and the device never parses HTTP.
- **Fonts and assets**: a subset 16 px Chinese font generated by a script in the
  firmware branch (`tools/intercom_font.py`); the app icon is generated from a
  committed master image by `tools/make_android_icons.py` in the app repository.

## Version history

- **v1.8.0-intercom** — first release: BLE voice uplink, phone-side gateway, text on the device screen.
- **v1.10-intercom** — firmware and app report their versions and warn on a mismatch (both sides, non-blocking);
  the device settings menu hint line no longer uses a glyph the 16 px font lacks.
- **v1.11–1.12-intercom** — device speech (TTS) synthesized by the phone downlink; the app's settings page became a
  two-level menu; the device card shows the connected firmware version; app settings always shows the app version with
  an update check; long replies scroll eight lines at a time and are cut with a "……" marker.
- **v1.13-intercom** — a short OK press only wakes the screen (a release within 350 ms sends `turn_cancel`, so the
  phone no longer answers "no speech"), recognition starts the moment the key goes down (`turn_start` is no longer
  delayed), and a new message stops at its own first line while scrolling to the end leaves the last message
  bottom-aligned.

## What was verified

- Measured on a real device: press-to-green 280–290 ms; Opus uplink around
  3 KB/s; a complete turn from speech to on-screen reply over both an
  OpenClaw gateway and an OpenAI-compatible endpoint; pairing with the 6-digit
  code; message history scrolling.
- Released as `v1.8.0-intercom` and published to the community market as project
  799, including a real device photograph and app screenshots.
- **Not covered**: Bluetooth range, battery runtime, very long sessions, the
  number of concurrent turns, and Hermes end to end (its API server was not
  exposed to the local network at the time). Text-to-speech is implemented in a
  dormant code path only and is not exposed in the UI.

## Source

- Firmware: branch [`feature/openclaw-intercom`](https://github.com/Shinku-Chen/ai-passport/tree/feature/openclaw-intercom),
  release [`v1.8.0-intercom`](https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.8.0-intercom).
- Phone app: <https://github.com/Shinku-Chen/ai-passport-openclaw-android>
  (Android 8 and newer; signed releases built by CI).
- Community play: <https://ai-passport.folotoy.cn/plays/799/>.
- The device appearance used on the icon and cover is the AI Passport hardware;
  the character shown on its screen is the OpenClaw mascot and is not covered by
  any license granted by this project.
