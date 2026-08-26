<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# AI Passport Brand Visuals

This directory holds official product and brand visual references for the AI Passport. Use these images as the visual baselines when generating marketing or showcase renders with AI image tools.

## Overview

The AI Passport is a wearable device (named **Folotoy** on the device and in the community). The references cover the physical product from the front and back, plus three brand-colored shell renders of the front. Each image keeps the product silhouette, screen layout, and port/button placement identical; the EVA variants only change the shell color.

## Images

### Product references

| File | View | Notes |
| --- | --- | --- |
| [`ai-passport-front.png`](ai-passport-front.png) | Front | Transparent shell; 22:02 / MON / 85% status, Folotoy title, a character avatar, `Token 666 / 40000`, `GAME` and `IMAGE` buttons, and the tagline `The Open Wearable AI Passport`. |
| [`ai-passport-back.webp`](ai-passport-back.webp) | Back | Transparent shell over the PCB; `FOLOTOY` logo, `AI PASS WEARABLE DEVICE`, power/STA/BATT/USB/NFC LEDs, an `NFC` label, the `AI PASSport` title with `Wear it. Flash it. Make it anything.` and a QR code. |

### Brand-color shell renders (front)

| File | Color | Model badge | Notes |
| --- | --- | --- | --- |
| [`ai-passport-front-eva-01.png`](ai-passport-front-eva-01.png) | Purple | `01` | Unit-01 (EVA) colorway; `TEST TYPE` badge. |
| [`ai-passport-front-eva-00.png`](ai-passport-front-eva-00.png) | Orange | `00` | Unit-00 (EVA) colorway; `PROTOTYPE MODEL` badge. |
| [`ai-passport-front-eva-02.png`](ai-passport-front-eva-02.png) | Red | `02` | Unit-02 (EVA) colorway; `PRODUCTION MODEL` badge. |

All three color renders are 1024 × 1536 PNG and share the same layout as the standard front (`ai-passport-front.png`): 22:02 / MON / 85%, `TOKEN 666 / 40000`, a `SYNC LEVEL` meter, and `ENTRY` / `SYNC` side labels.

## Image dimensions

| File | Format | Dimensions | Size |
| --- | --- | --- | --- |
| `ai-passport-front.png` | PNG | 605 × 931 | 460 KB |
| `ai-passport-back.webp` | WebP | — | 118 KB |
| `ai-passport-front-eva-01.png` | PNG | 1024 × 1536 | 2.0 MB |
| `ai-passport-front-eva-00.png` | PNG | 1024 × 1536 | 2.0 MB |
| `ai-passport-front-eva-02.png` | PNG | 1024 × 1536 | 2.0 MB |

## How to generate new renders

When generating a marketing or showcase render from these references, keep the following consistent:

- **Silhouette**: rounded-corner wearable card with a top key-ring hole and side buttons.
- **Screen**: a single centered rectangular display showing the time/date, title, `Token` value, and two action buttons.
- **Logo and label text**: keep `Folotoy` / `FOLOTOY`, the model name, and any tagline as shown.
- **Colorways**: use the shell reference as the base and change only the accent/shell palette for a new variant (for example a white, green, or black edition).
- **Aspect**: renders at a 2:3 vertical ratio read best for the front view.

Unless the reference image is licensed for reuse, use these files as an internal visual baseline rather than recycling them verbatim into published assets.

## Usage

- Because these are attached through the session, reference the relevant `cindy-media://` blob or the local file when generating images in an AI tool.
- Document any new render you create here by adding a row to the tables above and linking the generated file in this directory.
- Keep product facts (labels, model names, status text) accurate; do not invent hardware features that are not present in the references.
