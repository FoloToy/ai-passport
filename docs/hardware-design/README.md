<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Hardware Design

This directory contains the authoritative hardware documents for every standard-production AI Passport.

## Documents

- [specifications.md](specifications.md): user-visible dimensions, devices, power, input, and battery specifications.
- [AI_HARDWARE_DEVELOPMENT_GUIDE.md](AI_HARDWARE_DEVELOPMENT_GUIDE.md): firmware-visible pins, ownership, electrical facts, constraints, troubleshooting, and device acceptance.
- [`bsp_pins.h`](../../components/bsp/include/bsp_pins.h): firmware constants used by BSP code.

Do not infer an interface from the ESP32-C3 datasheet alone. A hardware change updates `bsp_pins.h`, the hardware guide, and its physical acceptance result in the same PR.
