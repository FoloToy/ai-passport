<p align="right">
  <a href="CI-build-and-release.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Automated Build and Release

`.github/workflows/build-firmware.yml` builds and publishes firmware for tags and supports manual dispatch. Ordinary branch pushes do not trigger it. Keep this page synchronized with the workflow.

The build job restores ccache, runs `./tools/validate.sh --firmware` with ESP-IDF 5.5.3 for ESP32-C3, verifies the bootloader at `0x0`, partition table at `0x8000`, application at `0x10000`, and 8 MB Flash arguments, then uploads `FoloToy-AI-Passport-full.bin`. A separate least-privilege release job publishes that artifact only for a tag.

All Actions are pinned to full commit SHAs. The build job has `contents: read`; only the tag release job receives `contents: write`.

## Browser flashing

Open `https://ai-passport.folotoy.cn/tools/web-flasher/`, connect the USB JTAG/serial device, select the release's merged `FoloToy-AI-Passport-full.bin`, choose a baud rate such as 460800, and write it from `0x0`. The browser performs local writing and verification; it does not upload the firmware file.

For board and flashing details, see [the hardware development guide](../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md).
