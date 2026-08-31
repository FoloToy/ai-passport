<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# AI Passport Senior Safety Card

Senior Safety Card turns FoloToy AI Passport into an offline, wearable help
card for an older adult who may become lost or need assistance. It can show the
wearer's name and help message, family contacts, approximate home area, health
notes, and a family member's WeChat QR code.

## How it works

1. On first boot, scan the Wi-Fi QR code shown by the device.
2. The phone joins a password-protected local hotspot and opens the setup page.
3. Enter the safety information and optionally upload a family WeChat QR code.
4. Save the profile. The device stores it locally and immediately turns Wi-Fi
   off.
5. Use UP and DOWN to change pages. On the WeChat page, press OK to show the QR
   code; press any key to return.
6. Hold OK to reopen local setup. A configured management PIN is required by
   the setup page.

The normal display enters deep sleep after one minute without input. The setup
hotspot sleeps after five minutes without input. Any function key wakes the
device.

## Privacy and safety

- Profile data stays in the device's NVS partition and is never uploaded to an
  internet service.
- The family QR image stays in the device's `imgstore` partition.
- A management PIN is stored only as a random salt and SHA-256 digest.
- Full home address display is opt-in. Phone numbers are always shown in full so
  a helper can contact the family immediately.
- Wi-Fi and its local web server run only during setup. Bluetooth is disabled
  during normal application use.

This remains a physical card that other people can read. Do not enter an ID
number, bank information, a door code, or other high-risk secrets. This is not
a medical device and does not replace emergency services or professional care.

## Compatibility

This version is based on the current `FoloToy/ai-passport` template and keeps
mini-program BLE installation compatibility:

- ESP32-C3 with 8 MB Flash and a 3 MB application partition
- protected `cardid` at `0x356000`
- permanent Recovery at `0x700000`
- hold UP for five seconds during boot to enter Recovery

The NVS namespace/key and `imgstore`/`imgframe` addresses match the original
Senior Safety Card release, so an existing profile and QR image can be reused
when the partition contents are preserved.

## Build and install

Use ESP-IDF 5.5.3 and run:

```bash
source /path/to/esp-idf/export.sh
./tools/validate.sh
```

Publish and install only `build/FoloToy-AI-Passport-full.bin`. On a provisioned
device, prefer the AI Passport mini-program Recovery flow. Never use
`erase-flash`; it destroys per-device identity and the permanent Recovery.

Build success is not device validation. Confirm the first-setup flow, all five
pages, QR scanning, PIN protection, one-minute sleep, key wake, and Recovery
entry on physical hardware before treating a release as hardware-verified.
