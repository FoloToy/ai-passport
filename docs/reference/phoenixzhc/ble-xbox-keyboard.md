<p align="right">
  <a href="ble-xbox-keyboard.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# AI Passport BLE Xbox Controller and Keyboard Integration Lessons

Connecting a Bluetooth input device to ESP32-C3 requires separate checks for discovery, pairing, HID services, report parsing, and application input. A visible device name, an encrypted link, or a steady controller LED cannot alone prove that buttons reach the application correctly.

These lessons come from [FoloToy-GameBoy][gb] and [FoloToy-FC][fc] and target AI Passport developers using ESP-IDF, Bluepad32, and BTstack. They cover observed failures, fixes, and reusable tests. They do not describe capabilities guaranteed by the upstream default firmware or universal Bluetooth device compatibility.

## 1 Environment and device results

| Item | Scope |
| --- | --- |
| Hardware | AI Passport, ESP32-C3, 8 MiB Flash, no PSRAM |
| Development environment | ESP-IDF 5.5.3 |
| GameBoy source | `f221fe83fc40ec07f37f93bb19ce3d74879e8718`, version 1.4 |
| FC source | `d1206fae4feb650a1c7d2a7a211cda4205ce3381`, version 1.0 |
| Bluepad32 origin | `e9b755faabc240585da42e6d26164bb2cdd064d3`, with local patches |
| BTstack origin | `5d4d8cc7b1d35a90bbd6d5ffd2d3050b2bfc861c`, with local patches |
| Concurrent connections | One input device: either a controller or a keyboard |

Nine relevant files are identical between these revisions: `gamepad.c`, `gamepad_discovery.c`, `gb_input.c`, and the BLE advertisement, HID classification, initialization, keyboard parser, and SMP files discussed below. The implementation can be shared; a device test in one application does not automatically validate the other application.

| Device or test | Confirmed result | Boundary |
| --- | --- | --- |
| Xbox Wireless Controller 1914, controller firmware 5.22.16.0 | Historical GameBoy device records confirm bonding, HID input, stored-key re-encryption after restart, and one automatic reconnection after disconnecting during a game | Does not validate all Xbox models, every subsequent application revision, or repeated reconnection endurance |
| MCHOSE G87 V2 Bluetooth keyboard | On 2026-10-09, the project author confirmed that physical keyboard testing worked normally in GameBoy | Application firmware version and separate passkey, chord, and reconnection results were not recorded; this is not an FC device result |
| IINE-1001 | Historical GameBoy records confirm pairing, directional input, and the board-button Start/Select fallback | Does not apply to other models such as IINE L167 or establish reconnection endurance |
| Complete FC BLE acceptance | FC reuses the GameBoy input implementation | The FC stage record still lists full pairing, button combinations, and disconnection/reconnection as pending |

Xbox and IINE details are in the [GameBoy validation record][gb-validation]. Its pinned statement that no physical keyboard had been tested is superseded by the G87 V2 author feedback above. Unrecorded individual test cases remain unconfirmed.

## 2 Establish that the device actually uses BLE

ESP32-C3 supports Bluetooth LE, but not Bluetooth Classic BR/EDR. [Espressif documents this hardware boundary][idf-ble]. Changing scan filters, accepting another name, or adding an HID parser cannot add Classic support to C3.

Labels such as Bluetooth 5.x, Xbox-compatible, tri-mode, or phone-compatible do not establish the transport. Record the physical model, controller firmware, and selected operating mode, then check the actual protocol. Product variants and operating modes can differ.

[Bluepad32's Xbox documentation][bp-gamepads] distinguishes firmware transports: earlier Xbox 3.x/4.x firmware uses BR/EDR, while the 5.x family uses BLE. The project result here specifically covers **1914 with 5.22.16.0**; it is not a promise for every device carrying an Xbox label.

A keyboard must also be in Bluetooth mode. This implementation has no USB HID or 2.4 GHz receiver path. Even with BLE transport, the device must expose HID services and reports the implementation can handle.

## 3 Give one Host ownership of the Bluetooth controller

Both projects use this input path:

```text
ESP32-C3 BLE Controller
  → BTstack (GAP, SMP, GATT, HID client)
  → Bluepad32 (device identification and HID parsing)
  → gamepad.c (current device, pairing state, input snapshots)
  → gb_input.c (game controls and menu edges)
  → application
```

The configuration starting point from [sdkconfig.defaults][config] is:

```ini
CONFIG_BT_ENABLED=y
CONFIG_BT_CONTROLLER_ONLY=y
CONFIG_BT_NIMBLE_ENABLED=n
CONFIG_BLUEPAD32_PLATFORM_CUSTOM=y
CONFIG_BLUEPAD32_MAX_DEVICES=1
CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE=n
CONFIG_BTSTACK_AUDIO=n
```

These settings depend on the project's Bluepad32 and BTstack components; they are not a complete port for an arbitrary ESP-IDF project. IDF supplies the controller and BTstack owns the Host. Do not also start the baseline NimBLE advertising demo against that controller. Check component dependencies, BTstack configuration, and the actual initialization entry point together.

`gamepad_start()` creates a Bluetooth task that calls `btstack_init()`, `uni_platform_set_custom()`, and `uni_init()` before entering the BTstack event loop. Input callbacks update snapshots consumed by the game loop. Emulator execution, audio, and UI rendering stay outside Bluetooth callbacks. [Implementation][gamepad]

## 4 Investigate discovery rules before blaming the radio

### Active scanning still needs advertisement merging

The name, Appearance, and service UUIDs can arrive in separate advertising and scan-response packets. Requiring the name and type in every individual packet discards valid devices. Active scanning alone does not fix that.

The project enables active scanning with `gap_set_scan_parameters(1, 48, 48)` and merges fields in a cache. The two `48` values use BTstack scan parameter units; they are not milliseconds or a universal power recommendation.

The [advertisement implementation][advertisement] follows these rules:

- Key the lower-level cache by **address and address type**. Keep at most 16 entries, expire entries after five seconds without updates, and reset on scan stop.
- Accept either packet order and update names when more information arrives. A shortened name must not replace a complete name.
- Validate AD field lengths before updating the cache, so truncated packets cannot corrupt earlier valid information.
- Truncate names at UTF-8 character boundaries.

The advertisement cache and application candidate list are separate states. The application list holds eight candidates and currently matches by address; it does not propagate address type throughout the application. It is not a general identity registry that has solved random-address rotation or identity resolution. Applications needing those features must retain identity information end to end and add device tests.

### A device name is not a compatibility allowlist

The early application required an Xbox name and excluded controllers with other names. The fix removed that requirement while retaining explicit user selection before connection. Names serve presentation; protocols and reports establish whether a device is usable.

Accepting only Gamepad also misses devices advertising Joystick. The current mapping is below. COD here is an internal Bluepad32 classification, not a requirement for a BLE advertisement to contain a Classic Bluetooth COD.

| BLE Appearance | Meaning | Internal COD |
| --- | --- | --- |
| `0x03C1` | Keyboard | `0x0540` |
| `0x03C2` | Mouse | `0x0580`, rejected by this application |
| `0x03C3` | Joystick | `0x0504` |
| `0x03C4` | Gamepad | `0x0508` |
| Missing, zero, or `0x03C0`, with an HID service | HID candidate awaiting classification | `0x0500` |

### An HID service UUID permits further inspection

Some devices omit a specific Appearance but advertise HID service `0x1812`. The implementation recognizes complete and incomplete lists of 16-bit UUIDs and Bluetooth-base 128-bit UUIDs. This fallback does not override an explicitly different type such as Mouse.

After selection and connection, a UUID-only candidate must pass Report Map inspection. The [classifier][hid-classifier] accepts top-level Gamepad, Joystick, or Keyboard Application collections containing data Input items and rejects unsupported, malformed, or conflicting classifications. It then selects the first suitable HID service, copies its descriptor before parser initialization, and ignores reports before classification or from other services. [Service handling][ble]

This additional classification path applies to UUID-only candidates with a pending type. Existing known devices did not all move to a new parser path. Devices advertising neither a recognized type nor an HID UUID can still be absent from the list.

## 5 Investigate pairing when Xbox connects without input

### Encryption does not establish completed bonding or input

During development with Xbox 1914, the non-bonding configuration established encryption while the controller LED kept flashing and no button reports arrived. The tested combination ultimately used Legacy Just Works with bonding:

```c
sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
sm_set_authentication_requirements(SM_AUTHREQ_BONDING);
```

Historical device results then confirmed a steady LED, HID service connection, input reports, and stored-key re-encryption after restart. This is a result for the stated model and firmware, not a universal security policy. Just Works does not provide MITM authentication; products requiring it need an appropriate separately validated policy. Keyboard IO capabilities also differ.

Read executing statements rather than relying on old experiment comments. [`uni_bt_le_setup()`][ble] retains comments about several pairing combinations; the active `sm_set_authentication_requirements()` call determines the configuration.

### Inconsistent key distribution can stall pairing

The project applies two related changes to [BTstack SMP][sm]:

1. Do not request long-term bonding keys when bonding is not requested.
2. When acting as initiator, intersect key-distribution bits in the response with the original local request.

The second change includes:

```c
keys_to_send &= sm_pairing_packet_get_initiator_key_distribution(setup->sm_m_preq);
keys_to_receive &= sm_pairing_packet_get_responder_key_distribution(setup->sm_m_preq);
```

A longer timeout cannot complete a state machine waiting for keys that were not requested and will not arrive. Inspect Pairing Complete, Re-encryption Complete, Device Information discovery, HID Service Connected, and actual Input Reports separately to locate the stalled stage.

These are local changes against pinned dependencies. Compare newer upstream implementations before reapplying them. The focused host tests in this article do not exercise the complete SMP radio exchange; pairing still requires real devices.

## 6 Implement the keyboard passkey interaction

A keyboard differs from a controller with no input capability in more than its mapping table. For recognized keyboards and HID candidates awaiting classification, the application selects `IO_CAPABILITY_DISPLAY_ONLY` so it can display a passkey when the peripheral requests one.

The [implementation][gamepad] handles `SM_EVENT_PASSKEY_DISPLAY_NUMBER`, `SM_EVENT_PASSKEY_DISPLAY_CANCEL`, `SM_EVENT_PAIRING_COMPLETE`, and `SM_EVENT_REENCRYPTION_COMPLETE`. Display all six digits, including leading zeros; the user types them on the keyboard and presses Enter. Pairing Enter is independent of the game's Start mapping.

Handle these boundaries together:

- On a passkey event, extend the device's original 20-second connection timer to 60 seconds and maintain a separate 60-second passkey-display timer.
- Show events only for the selected device. Another peripheral must not overwrite its prompt.
- Clear the prompt on completion, failure, cancellation, disconnection, and timeout. A disappearing prompt is not proof of successful pairing.
- After cancellation, cancel pending connections and clean up non-owner devices on the Bluetooth thread. A late ready callback must recheck selection.

Six digits, the 60-second window, and one concurrent device describe this implementation. The author's successful G87 V2 test does not establish separate passes for wrong passkeys, cancellation races, or repeated reconnections that were not individually recorded.

## 7 Parse keyboard reports using HID descriptors

### Do not assume every report is a fixed eight-byte packet

Keyboards can use key arrays, reports prefixed by a Report ID, or bitmaps with one bit per key. Fixed byte offsets can mistake a Report ID for a key or drop chords. These projects use Bluepad32 and BTstack descriptor parsing, then map Keyboard/Keypad Usage values to application actions.

The example application mapping is:

| Keys | HID Usage | Game action |
| --- | --- | --- |
| W / A / S / D | `0x1A` / `0x04` / `0x16` / `0x07` | Up / Left / Down / Right |
| J / K | `0x0D` / `0x0E` | A / B |
| L / I | `0x0F` / `0x0C` | Start / Select |

These are physical key usages, independent of case, input methods, and character entry. Other applications can change the mapping without rewriting the HID parser. The controller application path currently reads D-pad, A/B, and Select/Start; it does not automatically map analog stick axes to directions.

### A separate media report must not release held keys

A failure easily missed by single-key tests occurs when D and J remain held while the keyboard sends a volume report. If the parser clears the keyboard state at the start of every report, the application incorrectly sees the direction and action key released.

The [local fix][keyboard-parser] resets the parsing cursor at report start for ordinary keyboards and rebuilds keyboard state only when a Keyboard/Keypad usage page is encountered. Independent Consumer/media reports retain held letter keys; a subsequent keyboard release report still clears them. The existing special JX-05 branch remains separate.

The [real-parser host test][keyboard-test] exercises D+J+K held, media press, media release, and finally all letter keys released. It also covers arrays with and without Report IDs and bitmap reports. This fixes interference from separate media reports; it does not establish arbitrary multi-Report-ID state merging or unlimited NKRO compatibility.

### Define invalid-report and release behavior

[`gb_input.c`][input] treats `ErrorRollOver`, `POSTFail`, and `ErrorUndefined` usages 1 through 3 as invalid input and releases game keys rather than leaving a direction stuck. Opposite directions cancel each other. Ordinary release reports replace the current key set, and disconnection clears the whole input snapshot.

Games need held state; menus need press edges. On menu entry or reconnection, initialize the edge detector from the current snapshot so an already-held confirmation key does not activate a menu item. Keyboard repeat reports must not become repeated menu confirmations.

## 8 Thread ownership and lifecycle matter as much as the protocol

Keep Bluepad32 `*_unsafe()` calls on the BTstack thread. UI and game tasks submit requests using `btstack_run_loop_execute_on_main_thread()`. Recheck current connection state when applying scan policy, so an old queued request cannot restart scanning immediately after a device becomes ready.

The implementation protects input and discovery snapshots with short critical sections and accepts reports only from `s_owner`. Do not render or perform blocking I/O while holding these locks. On disconnection, clear the owner and all buttons before restoring scanning according to policy. Clear old candidates when starting a fresh discovery session or cancelling selection, so the eight-slot list does not remain full of stale devices.

The projects disable BLE scanning while in Wi-Fi management and restore it afterward. Stopping scans neither powers off the controller nor disconnects an existing peer, and it does not validate heavy Wi-Fi/BLE coexistence. On C3 without PSRAM, observe free internal heap, the largest free block, Bluetooth task stack usage, and callback duration before choosing resource settings.

A frozen game with continuing audio or an unexpected return to the home screen is not automatically a Bluetooth disconnection. GameBoy records include such symptoms, with input reports still arriving during some observations. Locate the failure using disconnect events and report timestamps, then investigate the emulator, storage, audio, and scheduling separately. A BLE fix is not evidence that all application stalls were resolved.

## 9 Diagnose from the last successful stage

Track the following sequence. This is a diagnostic model, not an existing unified state enum in the projects:

```text
Controller ready → advertisement received → candidate list → user selection
→ link established → pairing or re-encryption complete → HID service ready
→ report parsed → application action → release on disconnect and reconnect
```

| Symptom | Inspect first | Fix or validation direction |
| --- | --- | --- |
| No device in the list | Transport, operating mode, pairing mode, raw advertisements | Establish BLE support; accepting a name cannot add BR/EDR |
| Advertisements arrive but no candidate appears | Split fields, Appearance, HID UUID, name filtering | Merge advertising and scan responses; accept Joystick as well as Gamepad |
| List stays full of old devices | Eight-slot application state | Reset on a fresh scan or cancellation; preserve an active selection |
| Xbox link encrypted, LED flashing, no input | Bonding policy and SMP distribution | Check the model-specific result, pairing completion, and HID reports |
| Keyboard waits and then times out | IO capability, passkey events, connection timer | Display six digits, allow typing time, retain cancellation |
| Media key releases a game direction | Per-report state reset | Separate Keyboard/Keypad state from Consumer reports |
| Character moves after disconnect | Owner cleanup and full snapshot release | Test powering off while a direction is held |
| Menu immediately confirms or skips items | Edge baseline and repeat reports | Initialize menu state on entry and reconnection |
| HID UUID exists but connection fails | Report Map, selected HID service, Device Information discovery | UUID is only candidate evidence; this code still disconnects on failed device-information discovery |
| Startup reports `0x0c05` with status `0x01` | BR/EDR initialization sent to C3 | Skip SSP and inquiry filters according to capability while completing BLE initialization |

The final row was an independent observed failure. `HCI_Set_Event_Filter` filters Classic inquiry results; the BLE-only controller returned Unknown HCI Command. [`uni_bt_setup.c`][setup] now skips those commands based on build capability and runtime configuration rather than suppressing the error. Older logs still showed BLE scanning after the error, so the error alone did not prove scanning had failed. [Historical record][gb-validation]

Distinguish pairing, re-encryption, service discovery, ready, input, disconnection, and reconnection in logs. Record model, firmware, input mode, failure stage, and status codes. Remove addresses, keys, and other device identifiers before sharing logs; do not publish connection keys.

## 10 Validate state transitions

On 2026-10-09, the following seven host programs were rebuilt and run against the pinned GameBoy source: **7 passed, 0 failed**. They used host GCC, C11, and `-O2 -Wall -Wextra -Werror`, executing project logic with existing stubs for radio/platform dependencies. They do not perform physical pairing or cover the full SMP exchange.

| Test program | Main coverage |
| --- | --- |
| `test_gamepad_discovery` | Candidates, selection, cancellation, refresh, PIN state |
| `test_bt_le_advertisement` | Split packet order, address-type isolation, expiry, malformed AD, HID UUIDs |
| `test_bt_le_hid` | Supported top-level collections, conflicting types, truncation, bounds |
| `test_gb_input` | Mapping, chords, invalid usages, release, menu edges |
| `test_gb_keyboard_hid` | Real HID parser, Report IDs, media keys, arrays and bitmaps |
| `test_bt_setup` built for ESP32C3 | BLE-only initialization |
| `test_bt_setup` built for ESP32 | Dual-mode initialization and runtime-disabled BR/EDR |

See [test sources][tests] and [validate.sh][validate] for the build entry points. The count describes seven program runs, not seven device tests or a complete project gate. No firmware was rebuilt or flashed while preparing this article.

For each new physical device, record these acceptance cases separately:

- First pairing: correct peripheral mode, selectable candidate, then actual press and release reports.
- Keyboard passkey: with/without a code, leading zeros, wrong code, timeout, cancellation, and late events after cancellation.
- Input: direction/action chords, holds, partial and complete release, intervening media reports, opposite directions.
- Disconnection: power off or move out of range while holding a direction; release application input and avoid menu activation after reconnecting.
- Bonding: restart peripheral and host independently, reuse existing keys, and pair again; record reconnection count and observation duration.
- Application load: concurrent game, sound, and display, including entry into and exit from Wi-Fi management.

Attach the application revision or firmware identity, peripheral model/firmware/mode, and test conditions to results. A report of normal real-world use is valid feedback, but does not replace unexecuted failure-path tests. The FC simulator test entry does not execute the production BLE path, so simulator passes cannot establish wireless acceptance. [FC validation scope][fc-validation]

## 11 Reuse the implementation by layer

| Capability | Pinned source entry points |
| --- | --- |
| Candidates, selection, PIN, cancellation, snapshots, thread dispatch | [`main/gamepad.c`][gamepad], [`main/gamepad_discovery.c`][discovery] |
| Input mapping and menu edges | [`main/gb_input.c`][input] |
| Advertisement merging and HID UUID fallback | [`uni_bt_le_advertisement.c`][advertisement] and its header |
| Descriptor classification and service selection | [`uni_bt_le_hid.c`][hid-classifier], [`uni_bt_le.c`][ble] |
| BLE-only initialization | [`uni_bt_setup.c`][setup] |
| Keyboard state across media reports | [`uni_hid_parser_keyboard.c`][keyboard-parser] |
| SMP key-distribution changes | [`components/btstack/src/ble/sm.c`][sm] |

Start a port with a minimal screen showing device state and an input snapshot. Validate discovery, pairing, and release before connecting business logic. Bring the relevant headers, build dependencies, and tests, and retain third-party origins, licenses, and local-change records. The GameBoy/FC emulator, ROMs, partition layout, and full Flash images are not prerequisites for BLE input.

[gb]: https://github.com/PhoenixZHC/folotoy-gameboy/tree/f221fe83fc40ec07f37f93bb19ce3d74879e8718
[fc]: https://github.com/PhoenixZHC/folotoy-fc/tree/d1206fae4feb650a1c7d2a7a211cda4205ce3381
[gb-validation]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/docs/development/gameboy-validation.md
[fc-validation]: https://github.com/PhoenixZHC/folotoy-fc/blob/d1206fae4feb650a1c7d2a7a211cda4205ce3381/docs/validation.md
[idf-ble]: https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32c3/api-guides/ble/overview.html
[bp-gamepads]: https://bluepad32.readthedocs.io/en/latest/supported_gamepads/#xbox-wireless-model-1914-3-buttons
[config]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/sdkconfig.defaults
[gamepad]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/main/gamepad.c
[discovery]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/main/gamepad_discovery.c
[input]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/main/gb_input.c
[advertisement]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/components/bluepad32/bt/uni_bt_le_advertisement.c
[hid-classifier]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/components/bluepad32/bt/uni_bt_le_hid.c
[ble]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/components/bluepad32/bt/uni_bt_le.c
[setup]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/components/bluepad32/bt/uni_bt_setup.c
[keyboard-parser]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/components/bluepad32/parser/uni_hid_parser_keyboard.c
[keyboard-test]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/tests/test_gb_keyboard_hid.c
[sm]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/components/btstack/src/ble/sm.c
[tests]: https://github.com/PhoenixZHC/folotoy-gameboy/tree/f221fe83fc40ec07f37f93bb19ce3d74879e8718/tests
[validate]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/tools/validate.sh
