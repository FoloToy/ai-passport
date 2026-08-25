<p align="right">
  <a href="brand-and-product.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Brand and Product Information

This document defines public brand and product language. For engineering facts, use [product specifications](hardware-design/specifications.md), the [hardware guide](hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md), and `components/bsp/include/bsp_pins.h`.

## Brand and positioning

- Product name: **AI Passport**; full English name: **FoloToy AI Passport**. Do not translate or invent variants of the product name.
- Brand: **FoloToy**, whose GitHub organization is `github.com/FoloToy`.
- Repository: `github.com/FoloToy/ai-passport`, the open development baseline for confirmed hardware facts, stable interfaces, resource limits, reference implementations, and acceptance methods.
- Positioning: **Open Wearable AI Agent** — a wearable, open AI agent that users can redefine through installable plays.
- Core message: **WEAR · PLAY · CREATE**.

| Mode | Meaning |
| --- | --- |
| WEAR | Use it as an identity card, synchronizing a name, avatar, introduction, and images locally over Bluetooth LE. |
| PLAY | Install an official play from the browser while retaining identity-card functionality. |
| CREATE | Build custom firmware directly or with an AI coding agent. |

## Official links

| Resource | Address |
| --- | --- |
| Chinese product site | `https://ai-passport.folotoy.cn/` |
| English product site | `https://ai-passport.folotoy.cn/en/` |
| User guides | `https://ai-passport.folotoy.cn/guides/` |
| Getting started | `https://ai-passport.folotoy.cn/guides/getting-started/` |
| Official plays | `https://ai-passport.folotoy.cn/plays/` |
| AI Passport web flasher | `https://ai-passport.folotoy.cn/tools/web-flasher/` |
| General FoloToy web flasher | `https://tool.folotoy.cn/` |

Both flashers write local firmware through WebSerial without uploading the firmware file. Prefer the product-site flasher in AI Passport flows and the general tool for cross-product support.

The official play catalog changes over time; treat the live website as authoritative rather than copying a permanent list into engineering decisions.

## Specifications, source, and license

See [specifications.md](hardware-design/specifications.md) for dimensions, weight, battery, charging, NFC, input, and wireless specifications. The source repository is licensed under the MIT License, Copyright (c) 2026 FoloToy. See [fork-guide.md](fork-guide.md) for downstream development conventions.

This page owns public names, positioning, and official entry points. It does not override pin, bus, resource, or board behavior documented by the hardware sources.
