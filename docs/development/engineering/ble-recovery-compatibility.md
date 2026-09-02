<p align="right">
  <a href="ble-recovery-compatibility.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Mini-Program BLE Firmware Compatibility

This repository is a derivative-firmware template. Every application built from
it must remain installable by the AI Passport mini-program through the permanent
Recovery already provisioned by the official factory firmware.

## How installation works

The application does not implement the BLE flashing service. The user holds UP
while powering on for five seconds; the custom bootloader then starts the
factory-installed Recovery at `0x700000`. Recovery exposes the FFF0–FFF4 BLE
service, verifies the device, receives the application and resource sections,
protects device identity, commits a compatible partition table, and starts the
new application.

The community artifact is therefore an input to Recovery, not a replacement for
Recovery. A device whose permanent Recovery was erased must first be restored
with the official USB recovery flow.

## Mandatory contract

Derivative projects must preserve all of the following:

- ESP32-C3, 8 MB Flash, ESP-IDF 5.5.3.
- A merged ESP image starting at `0x0`, produced as
  `build/FoloToy-AI-Passport-full.bin`.
- One main application image at `0x10000`, no larger than `0x300000` bytes.
- `cardid`: data/NVS at `0x356000`, size `0x4000`.
- `recovery`: app/test at `0x700000`, size `0x100000`.
- The bootloader hook under `bootloader_components/recovery_boot_hook/`, which
  enters Recovery after the UP key is held for five seconds.
- A valid partition-table MD5 marker and no partition overlap with either
  protected region.
- No device-specific `cardid` payload and no replacement Recovery payload in a
  community artifact.

Applications may add resource partitions, but the partitions must not overlap
the protected regions. Required resource partitions must be included in the
merged artifact rather than declared empty.

## Enforced validation

Run:

```bash
./tools/validate.sh --firmware
```

The check builds in an isolated directory, creates the merged image, verifies
the bootloader/table/application offsets, parses the partition table, checks its
MD5 and protected ranges, enforces the 3 MB application limit, rejects protected
payload bytes, and confirms the Recovery boot hook is linked. CI runs the same
gate. Do not publish an artifact when this command fails.

Upload only `build/FoloToy-AI-Passport-full.bin`; the similarly named app-only
`build/FoloToy-AI-Passport.bin` cannot pass mini-program compatibility checks.

## Flashing safety during development

Never run `idf.py erase-flash` on a provisioned device. It destroys both the
per-device identity and permanent Recovery. Prefer mini-program installation or
the segmented `idf.py flash` development command, which does not write an image
for the protected partitions. A raw single-file write from `0x0` is safe only
when its byte range ends before `cardid`; a merged artifact containing later
resource partitions spans the gap and must not be raw-flashed to a provisioned
device.
