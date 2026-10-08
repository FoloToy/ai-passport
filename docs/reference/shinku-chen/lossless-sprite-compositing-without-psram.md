<p align="right">
  <a href="lossless-sprite-compositing-without-psram.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Compositing Full-Screen Scenes Without PSRAM: JPEG Base, Lossless Top Layers

Collected after releasing **ATRI Reader**, which draws 240 × 320 visual-novel scenes on
a part with no PSRAM: a background, a translucent text band, a full-body character
sprite and an effect overlay, all composited into one canvas before every frame.

## The decode buffer decides the design

With LVGL, the audio codec and the Wi-Fi/BLE stacks around, the free heap in this
application sits around 40 KB. A full-screen JPEG decode wants a working buffer of
its own, so the cheapest design is to let the **canvas be the decode destination**:
the background is decoded straight into the frame the user will see, and nothing else
is allocated for it.

That constrains the top layers: there is no room to decode a second full-screen image
per frame. They are stored losslessly instead.

| Layer | Count | Format | Cost |
| --- | --- | --- | --- |
| Backgrounds | 73 | JPEG q88, 240 × 320 | 1,395 KB total (about 19 KB each) |
| Effect overlays | 14 | RGB565 + 4 bpp alpha mask, cropped to the alpha bounding box | part of 1,322 KB |
| Full-body character sprites | 5 | RGB565 + 4 bpp alpha mask, source up to 750 × 920, cropped | part of 1,322 KB |

The script text (12,188 lines) is the third part of the 3.83 MB pack at 563 KB.

## What the lossless treatment buys

JPEG ringing is very visible on a sprite drawn over a flat background, and the
character is the thing the reader looks at. Cropping to the alpha bounding box keeps
the cost reasonable: the largest sprite in this title composites 101 × 295 pixels
(about 14 KB of pixels plus a quarter of that again for the mask), and one composite
touches roughly 13,700 pixels — a few milliseconds, well inside a frame budget. A
4 bpp mask (16 levels) is enough for clean edges at this size; 8 bpp would double the
mask cost for no visible gain.

Two details worth copying:

- Crop at pack time, not at draw time. Cropping in the packer also shrinks the
  committed pack, and the firmware then only ever handles the bbox.
- Anchor sprites to the edge of the art area (here 6 px from the right) rather than
  to the position in the source artwork, so a character never lands on top of the
  text the reader is reading.

## Verify layering without a camera

A serial command that renders an arbitrary scene into the art canvas and streams the
raw frame back (`ATRISHOT <chapter> <scene>`) is worth its flash: wrong z-order,
wrong offset or a stale sprite shows up immediately as a captured image, on a board
with no camera and no screenshot key.
