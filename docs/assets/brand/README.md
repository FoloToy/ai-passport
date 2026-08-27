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
| [`ai-passport-views.jpg`](ai-passport-views.jpg) | All views | Six-view product diagram (top, bottom, front, back, and both sides) with a blank screen; useful as an overview/exploded-view reference for generating a full-device render. |

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
| `ai-passport-views.jpg` | JPEG | 3849 × 2448 | 324 KB |
| `ai-passport-front-eva-01.png` | PNG | 1024 × 1536 | 2.0 MB |
| `ai-passport-front-eva-00.png` | PNG | 1024 × 1536 | 2.0 MB |
| `ai-passport-front-eva-02.png` | PNG | 1024 × 1536 | 2.0 MB |

## How to generate new renders

When generating a marketing or showcase render from these references, keep the following consistent:

- **Reference as the device base.** The reference image is the base for the whole render: keep its outer shell, top key-ring hole, side buttons, ports, and logo text as they are. The reference's on-screen area is only a placeholder — you do not keep it.
- **Redraw the screen area to the request.** The core operation is to redraw the reference's screen region to the content the user asks for (for example the actual UI of a specific play), keeping the screen's size, aspect ratio, corners, and position inside the shell identical to the reference. Keep the placeholder screen inset inside the matched frame; do not reshape, move, or replace the screen frame itself.
- **Logo and label text**: keep `Folotoy` / `FOLOTOY`, the model name, and any tagline as shown on the shell.
- **Colorways**: use the shell reference as the base and change only the accent/shell palette for a new variant (for example a white, green, or black edition).
- **Aspect**: renders at a 2:3 vertical ratio read best for the front view.

Unless the reference image is licensed for reuse, use these files as an internal visual baseline rather than recycling them verbatim into published assets.

## Tooling notes

When generating a render that must match a reference, the common failure mode is a model that "reimagines" the device because it never actually saw the reference image. Keep these points in mind regardless of the image tool you use:

- **Always pass the reference image explicitly.** Generating a device render is a reference-based redraw, not a from-scratch text-to-image: you must supply a `docs/assets/brand` reference (typically `ai-passport-front.png` or a colorway shell render) as an input to the generation call, keep its outer hardware, and redraw only the screen region to the requested content. Relying only on a text description lets the model invent a look-alike device instead of reproducing this one.
- **Local file paths are not accepted as image input.** Most generation services reject a bare filesystem path. Pass the image as a service-supported form: a hosted reference URL, a data URL, or whatever the tool documents for uploads/attachments.
- **Very long data URLs are fragile when passed by hand.** A multi-KB base64 data URL is easy to corrupt during manual copying between tool calls; prefer a short hosted reference or let the tool resolve the image itself.
- **Verify hardware fidelity after generating.** Compare the render against the reference for the shell silhouette, key-ring hole, side buttons, ports, and any model badge. If these drift, regenerate rather than accept the result.

## Usage

- Reference the local files in this directory when generating images with an AI tool; the source files are under `docs/assets/brand/`.
- Generated images are not brand assets by default. Unless the user explicitly requests a brand render, AI-generated images are project assets and belong in `assets/images/`, not here.
- Only register a render in this directory when the user specifically requests a brand visual: add a row to the tables above and link the generated file here.
- Keep product facts (labels, model names, hardware features) accurate; do not invent hardware that is not present in the references. On-screen status text is illustrative and not a fixed requirement.
