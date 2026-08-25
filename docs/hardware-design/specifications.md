<p align="right">
  <a href="specifications.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Product Specifications

This page defines the user-facing product specifications. Firmware pin assignments, buses, and runtime constraints are documented in the [hardware guide](AI_HARDWARE_DEVELOPMENT_GUIDE.md) and `components/bsp/include/bsp_pins.h`.

| Item | Specification |
| --- | --- |
| Form | Wearable with transparent enclosure |
| Dimensions | 60 × 95 × 8.5 mm |
| Weight | 50 g |
| MCU | ESP32-C3 with 8 MB Flash and no PSRAM |
| Display | ST7789P3, 240 × 320 color TFT, RGB565 |
| Wireless | 2.4 GHz Wi-Fi 802.11 b/g/n; Bluetooth 5 LE |
| NFC | Passive NTAG213 tag supporting ordinary NDEF read/write |
| Function input | UP, DOWN, and OK buttons on a GPIO0 ADC resistor ladder |
| Power input | Dedicated hardware power button |
| Power behavior | Hold power for 0.5 seconds to start and about 2 seconds to shut down; press a function button to wake after automatic screen-off |
| Audio | Built-in microphone and speaker with ES8311 codec |
| Charging | USB Type-C 2.0, 5 V input |
| Battery | Built-in 520 mAh rechargeable lithium battery with CW2017 fuel gauge |
| Other | Device-specific QR fallback with recovery-firmware entry |
