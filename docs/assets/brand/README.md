<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# AI Passport Brand Visuals

This directory holds official product and brand visual references for the AI Passport. Use these images as the visual baselines when generating marketing or showcase renders with AI image tools.

## Overview

The AI Passport is a wearable device (named **Folotoy** on the device and in the community). The references cover the physical product from the front and back, plus three brand-colored shell renders of the front. Each image keeps the product silhouette, port/button placement, and shell color important details. The screen content is not a fixed requirement — the AI Passport is a programmable wearable, so the screen may be fully replaced depending on the play or firmware. When generating a render, keep the outside hardware (shell, buttons, ports, key-ring hole) consistent and treat the on-screen content as flexible.

## Images

### Product references

| File | View | Notes |
| --- | --- | --- |
| [`ai-passport-front.png`](ai-passport-front.png) | Front | Transparent shell; on-screen content is illustrative and may be replaced. Keep the shell, buttons, ports, and key-ring hole when generating a matching render. |
| [`ai-passport-back.webp`](ai-passport-back.webp) | Back | Transparent shell over the PCB; `FOLOTOY` logo, `AI PASS WEARABLE DEVICE`, power/STA/BATT/USB/NFC LEDs, an `NFC` label, the `AI PASSport` title with `Wear it. Flash it. Make it anything.` and a QR code. |

### Brand-color shell renders (front)

| File | Color | Model badge | Notes |
| --- | --- | --- | --- |
| [`ai-passport-front-eva-01.png`](ai-passport-front-eva-01.png) | Purple | `01` | Unit-01 (EVA) colorway; `TEST TYPE` badge. |
| [`ai-passport-front-eva-00.png`](ai-passport-front-eva-00.png) | Orange | `00` | Unit-00 (EVA) colorway; `PROTOTYPE MODEL` badge. |
| [`ai-passport-front-eva-02.png`](ai-passport-front-eva-02.png) | Red | `02` | Unit-02 (EVA) colorway; `PRODUCTION MODEL` badge. |

All three color renders are 1024 × 1536 PNG and share the same shell layout as the standard front (`ai-passport-front.png`). Their on-screen content is illustrative and may be replaced; the screen image itself is not a requirement for a generated render.

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

- **External hardware**: keep the rounded-corner shell, top key-ring hole, side buttons, and ports consistent with the reference.
- **Screen content**: not a requirement. The screen is a programmable display and its image may be fully replaced by the play or firmware; leave it flexible in generated renders.
- **Logo and label text**: keep `Folotoy` / `FOLOTOY`, the model name, and any tagline as shown on the shell.
- **Colorways**: use the shell reference as the base and change only the accent/shell palette for a new variant (for example a white, green, or black edition).
- **Aspect**: renders at a 2:3 vertical ratio read best for the front view.

Unless the reference image is licensed for reuse, use these files as an internal visual baseline rather than recycling them verbatim into published assets.

## Usage

- Reference the local files in this directory when generating images with an AI tool; the source files are under `docs/assets/brand/`.
- Document any new render you create here by adding a row to the tables above and linking the generated file in this directory.
- Keep product facts (labels, model names, hardware features) accurate; do not invent hardware that is not present in the references. On-screen status text is illustrative and not a fixed requirement.
