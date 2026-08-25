<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Hardware Design

This directory contains product specifications, firmware-visible board facts, resource constraints, acceptance matrices, and troubleshooting knowledge.

- `components/bsp/include/bsp_pins.h` is the source of truth for firmware pin assignments and board constants.
- Product specifications are maintained in `specifications.md`; firmware behavior and constraints are maintained in the hardware guide.
- Do not infer board interfaces from generic ESP32-C3 capabilities or another development board.
- Pin, I2C, ADC, display, audio-clock, or other hardware mapping changes must update the relevant document and record the physical-device result.

## Document map

| Document | Audience and authority |
| --- | --- |
| [AI_HARDWARE_DEVELOPMENT_GUIDE.md](AI_HARDWARE_DEVELOPMENT_GUIDE.md) | Engineering reference for BSP behavior, resource ownership, constraints, validation, and troubleshooting. |
| [specifications.md](specifications.md) | Public product specifications. |
| `components/bsp/include/bsp_pins.h` | Firmware source of truth for pin assignments, bus instances, addresses, panel parameters, and button windows. |

New documents must state their scope, link software interfaces instead of duplicating constants, and keep build results separate from physical-device results.
