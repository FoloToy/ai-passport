<p align="right">
  <a href="specifications.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Product Specifications

This page defines user-facing specifications. Engineering details follow the [hardware guide](AI_HARDWARE_DEVELOPMENT_GUIDE.md) and `components/bsp/include/bsp_pins.h`.

| Item | Specification |
| --- | --- |
| Form | Wearable with transparent enclosure |
| Dimensions | 60 × 95 × 8.5 mm |
| Weight | 50 g |
| MCU | ESP32-C3 with 8 MB Flash |
| Display | 240 × 320 color TFT |
| Wireless | 2.4 GHz Wi-Fi 802.11 b/g/n; Bluetooth 5 LE |
| NFC | Passive NTAG213 tag supporting ordinary NDEF read/write |
| Input | UP, DOWN, and OK function buttons plus a dedicated hardware power button |
| Power behavior | Hold power for 0.5 s to start and about 2 s to shut down; press a function key to wake after automatic screen-off |
| Audio | Built-in microphone and speaker |
| Charging | USB Type-C 2.0, 5 V |
| Battery | Built-in 520 mAh rechargeable lithium battery |
| Other | Device-specific QR fallback with recovery-firmware entry |
