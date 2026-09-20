<p align="right">
  <a href="two-device-ble-link.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Two-Device BLE Link Between AI Passport Boards (No PSRAM)

Captured after the **Connect Four** two-device link feature was validated on two
boards (2026-09-18). These findings are general and upstream-benefiting: they
apply to any AI Passport application that wants two boards to talk to each other
without a phone.

> **Verification status.** Measured on two AI Passport boards running the same
> firmware. The roles, the handshake, a full match, rematch and peer-leave were
> exercised; heap numbers below are device logs, not estimates. Range, long
> sessions, three-or-more boards, and battery figures are **not** covered.

## Symmetric discovery keeps the roles off the UI

Neither board has to be told whether it is the host. Both advertise a custom
GATT service *and* scan for the peer's advertising name at the same time, then
decide who connects:

- Compare the peer's advertising address with the local address byte by byte; the
  **larger** address initiates, the other waits. The rule is antisymmetric, so
  both boards compute opposite results with no negotiation and no double
  connect. No "host / join" menu item is needed.
- A name prefix (`C4-`, plus two MAC bytes) is a simpler discovery filter than
  matching a 128-bit UUID inside advertising data, and it keeps both boards
  distinguishable in a scanner list. The UUID still belongs in the scan response
  so phones and generic BLE tools can identify the service.
- The 128-bit UUIDs follow the Nordic UART layout
  (`6e400001`/`6e400002`/`6e400003-b5a3-f393-e0a9-e50e24dcca9e`), which is what
  makes an off-the-shelf BLE tool or a phone able to act as the peer. NimBLE
  prints the service UUID in its discovery log, which is a quick way to confirm
  the layout is identical on both sides.
- After connecting, stop advertising; on disconnect, restart both advertising and
  scanning so the pair can reconnect without a reboot.

`CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1` is the right setting for 1:1 play, but it
also means the tiebreak rule must be strict: a multirole board that both scans
and accepts connections can otherwise end up in a connect race.

### Enable all four roles in sdkconfig

The link needs **peripheral + broadcaster** (to advertise itself) and **central +
observer** (to scan for the peer and initiate). The template trims its demo
configuration with `CONFIG_BT_NIMBLE_ROLE_CENTRAL=n` and
`CONFIG_BT_NIMBLE_ROLE_OBSERVER=n`; with either one off, the board silently never
finds or connects to the peer, because the corresponding GAP procedure simply
fails. Turn all four roles on (they are only paid for at runtime, when the link
is started) and keep `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1` for 1:1 play.

## Budget the RAM before choosing the feature set

Measured on the ESP32-C3 board (no PSRAM), with the game UI, audio task and LVGL
already running:

| Point | Free heap |
| --- | --- |
| Boot, after the application UI is up | ~191.9 KB |
| Just before `nimble_port_init()` | ~131.2 KB |
| Connected, link steady state | ~125.2–128.9 KB |
| After the link is stopped and its host task is torn down | ~192.0 KB |

So the link costs roughly **60 KB of heap to bring up** plus ~6 KB while a
connection is live, and it is fully returned when the link is stopped. That is
why the link is started on demand (when the player enters the link mode) rather
than at boot.

The related trap is a **static** full-frame screenshot buffer. One 320 × 240
RGB565 frame is 150 KB, and the largest contiguous free block at boot measured
only ~114 KB, so the buffer has to live in `.bss`, where it permanently costs
150 KB. BLE cannot coexist with it on this board:

- Measuring this honestly is what decides the shipping configuration. The
  application here ships the link build and keeps the serial-screenshot tool in
  an opt-in build (`-DC4_ENABLE_SCREENSHOT=ON`), and the screenshot build reports
  `BLE UNAVAILABLE` instead of failing in a confusing way.
- If you must capture screens *and* link, plan for it in the design (for example
  strip-wise rendering into a small buffer) — it is not a configuration toggle.

## Two NimBLE pitfalls that only show up on hardware

Both of these compiled cleanly and were only found from device logs:

1. **Every characteristic needs an `access_cb`, even a notify-only one.**
   `ble_gatts_chr_is_sane()` rejects a characteristic whose `access_cb` is NULL,
   and the whole service registration then fails with a bare
   `ble_gatts_count_resources rc=3` (`BLE_HS_EINVAL`). The heap was fine
   (195 KB free), which is exactly why the error is easy to misread as "out of
   memory". Give the notify characteristic a read callback (returning a couple of
   identification bytes is useful for phone-side debugging) and add a local check
   that reports which characteristic is missing its callback.
2. **Descriptor-discovery `EDONE` also arrives after a successful CCCD write.**
   On the central side, the descriptor-discovery callback ends with
   `error->status == BLE_HS_EDONE`. That does *not* mean "no CCCD found": it also
   fires when the CCCD was found and the subscribe write was already issued.
   Treating it as failure makes the board connect, subscribe, and immediately
   tear the link down in a loop (19 attempts in the captured log). Track a
   "CCCD found" flag and only fail the discovery genuinely.

## Initialize NVS before the radio

Without `nvs_flash_init()`, the controller/PHY cannot read its RF calibration
data and every start pays a full calibration:

```text
E phy_init: esp_phy_load_cal_data_from_nvs: NVS has not been initialized.
W phy_init: failed to load RF calibration data (0x1101), falling back to full calibration
```

After adding `nvs_flash_init()` (without erasing, so application data is never
destroyed), the first link start logs
`phy_init: Saving new calibration data ...` and the **next** start logs nothing
from `phy_init` at all — direct evidence that the cached calibration is being
reused. The BT/PHY path needs NVS even though the NimBLE host is configured with
`CONFIG_BT_NIMBLE_NVS_PERSIST=n`.

## Make delivery reliable in the application, not in BLE

Neither direction gives an application-level acknowledgement: peripheral writes
are notifications, and central writes can be sent without response for latency.
For a turn-based game a lost packet is not a lost frame, it is a permanently
desynchronized board, so the application needs its own layer:

- Frame: magic + version + type + `bit7 = has data` with a 7-bit sequence +
  `bit7 = ack valid` with the last accepted peer sequence + up to 3 payload
  bytes. One frame fits in the default ATT MTU with room to spare, and the
  on-wire sizes are easy to confirm in the log (`write ... len=8` for a 3-byte
  payload, `len=5` for an acknowledgement-only frame).
- Stop-and-wait: accept only the next expected sequence, re-acknowledge
  duplicates instead of delivering them twice, retransmit every 400 ms, and give
  up after 5 attempts with an explicit "peer lost" state.
- Carry the move counter in the payload and validate it when it arrives, so a
  desynchronization is detected instead of silently producing two different
  boards.
- Decide the first player from the connection role plus a shared match number
  (for example "central first, alternating each match"), so both boards derive
  the same answer without another round trip. When both sides propose the next
  match number, converge on `max()` — adopting the peer's value on both sides
  swaps it and desynchronizes the colors.
- Keep all of this in a pure-C module with host tests; the BLE transport stays a
  byte pipe.

## Tear-down and sleep

Stop the radio (`nimble_port_stop()` + `nimble_port_deinit()`, then delete the
host task) before `esp_deep_sleep_start()`, otherwise the radio stays powered.
Stopping the link is also what returns the ~60 KB to the heap. The peer sees a
disconnect, so give it a "peer left" screen that keeps searching — and do not let
the idle-sleep timer fire in the middle of a linked match.

## Testing it

- Two boards are required for the real acceptance run; the useful evidence is
  both serial logs: the initiating side logs `role=central`, the other
  `role=peripheral`, and both log the same match numbers with opposite colors.
- A PC-side BLE peer (Python plus `bleak`) removes the need for a second board
  when you are iterating on the protocol: connect to the advertising board as a
  central, speak the same frames, and answer its moves, and a full match can be
  played against one board. The peer tooling used for that (`tools/c4_peer.py`)
  ships with the two-device BLE transport rather than with this note.
