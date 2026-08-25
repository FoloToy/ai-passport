<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Hardware Design

This directory contains board facts, pin mapping, constraints, acceptance matrices, and troubleshooting knowledge.

- `components/bsp/include/bsp_pins.h` is the source of truth for facts in its scope; reference it instead of copying constants.
- Separate confirmed facts from unknowns. Request evidence instead of filling gaps with values from another board.
- Pin, I2C, ADC, display, audio-clock, or other hardware mapping changes must update the relevant document and record physical-device results.

## Documents

- [AI_HARDWARE_DEVELOPMENT_GUIDE.md](AI_HARDWARE_DEVELOPMENT_GUIDE.md): complete board development and troubleshooting guide.
- [specifications.md](specifications.md): public device specifications.

New documents must state the applicable board/revision and date and link software interfaces instead of duplicating them.
