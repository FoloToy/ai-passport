<p align="right">
  <a href="ble-voice-uplink-and-pairing.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# A BLE Voice Uplink Where the Phone Does the Networking

Captured after the **Pocket Intercom** phone link was validated on real hardware
(2026-10-01). These findings are general and upstream-benefiting: they apply to
any AI Passport application that wants speech to leave the board through a phone
instead of through Wi-Fi on the device.

> **Verification status.** Measured on one AI Passport board with one Android
> phone: pairing, a complete talk turn over two different gateways, press
> responsiveness and the uplink rate below are device or phone logs, not
> estimates. Range, battery runtime, multiple simultaneous phones, and long
> sessions are **not** covered.

## Why let the phone do the networking

The board has no PSRAM and only a fraction of a megabyte of free heap, while the
phone already holds the credentials, the speech-recognition session and the
gateway connection. Keeping the board a pure BLE peripheral means:

- no Wi-Fi provisioning flow has to exist on the device at all, which also removes
  the "how do I enter a password with three buttons" problem;
- gateway tokens and API keys exist in exactly one place — the phone — and are
  never sent over the link, so a leaked device image cannot leak them;
- the firmware stays free of HTTP parsing, TLS, JSON payload handling and the
  associated buffer sizing.

The trade-off is that the accessory is useless without the phone nearby, and the
phone becomes part of the latency path. Both were acceptable for a handheld
intercom; neither is acceptable for a standalone player.

## Link shape

- The transport is the Nordic UART Service layout
  (`6e400001`/`6e400002`/`6e400003-b5a3-f393-e0a9-e50e24dcca9e`), which keeps the
  device reachable from generic BLE tools and from any phone stack without custom
  UUID plumbing.
- Frames are `[A5 5A][TYPE][FLAGS][LEN:2 big-endian][payload]` with a magic
  re-sync rule, so a truncated write cannot desynchronise the receiver for good.
  More than one frame may arrive in a single notification, and one frame may be
  split across notifications; the parser therefore works from a ring buffer and
  carries partial frames across writes.
- The device is the peripheral and the phone is the central. Keeping that fixed
  avoids the double-connect tiebreak logic that a symmetric peer link needs.
- The link meters outbound audio frames instead of pushing them as fast as the
  encoder produces them; without that, a fast phone can starve the transport and
  latency spikes instead of degrading gracefully.

## Pairing

- LE Secure Connections with MITM protection and bonding, and a **6-digit**
  passkey generated on the device and shown on its screen. The passkey must be
  at least `100000`: values below that are rejected by the Android side.
- Bonding is stored, so the pairing flow happens once per phone. The pairing code
  is regenerated for every new pairing attempt, and stale bonds on the phone are
  the usual cause of "it pairs then immediately drops".
- Pairing runs before any credential is stored, so a factory-fresh device never
  holds a secret. There is no companion-app provisioning step that writes a key
  into the device; the device identity that a gateway may need is generated and
  kept on the phone.

## Audio uplink: Opus on a part without PSRAM

- Opus at 16 kHz mono, 60 ms frames (960 samples), complexity 0 and DTX enabled.
  The frames average roughly 180 bytes, so the uplink runs at about **3 KB/s** —
  about a tenth of 16 kHz PCM — which keeps the BLE link comfortably away from
  its throughput ceiling.
- Complexity 0 is deliberate on this class of part: higher settings consume the
  core that LVGL and the link task also need.
- The encoder runs on a task with a **static** stack (24 KB here). Repeatedly
  allocating an encode stack per recording session fragments the heap on a
  no-PSRAM part; a single resident task with a static stack removed that class of
  failure entirely.
- A raw PCM frame type stays in the protocol as a fallback path, so a link or
  encoder problem can be isolated without changing the framing.

## Responsiveness: red on press, green when ready

- The display reaction is driven by the **key-down event**, not by any network
  state: the screen turns red the moment the button is pressed, which is what
  tells the user that the device is listening to them.
- Green means "the gateway is ready, start speaking". It is published by the
  phone when its recognition channel is usable. Measured press-to-green is about
  **280–290 ms** on the reference phone.
- Because that reaction depends on the phone, it needs a bounded fallback: if the
  "ready" notification never arrives, the UI must return to a usable state on its
  own rather than sitting in the red state forever. A short timeout (800 ms in
  this application) is enough for the normal case and prevents a stuck screen
  when the phone is asleep or out of range.
- The phone side earns that number by keeping the recognition channel warm: a
  persistent session with a keepalive, revalidated periodically, so a press does
  not pay for a connection setup.

## What to copy, and what to watch

- Copy: a fixed peripheral/central split, NUS UUIDs, a length-prefixed frame with
  magic re-sync, metered audio flow, static stacks for the encoder, a protocol
  level PCM fallback, and a key-down-driven visual reaction with a bounded
  fallback timeout.
- Watch: expecting the link to behave like a socket (it is a notification stream
  with variable notification sizes), leaving the device as the central (it costs
  the tiebreak logic and scans that compete with the phone), and letting the
  "ready" state depend on anything the user cannot see — the fallback timeout is
  what keeps that honest.
