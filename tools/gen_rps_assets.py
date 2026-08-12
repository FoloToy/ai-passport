#!/usr/bin/env python3
"""Generate 240x320 RGB565-LE rock/paper/scissors firmware frames.

The source images live in assets/rps/source.  They are cropped to the display
aspect ratio and resized with nearest-neighbour sampling so the reference
pixel-art stays crisp.  LVGL consumes the raw output directly from flash.
"""

from pathlib import Path
import struct

from PIL import Image, ImageEnhance


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "rps" / "source"
PREVIEW_OUTPUT = ROOT / "assets" / "rps" / "firmware"
RAW_OUTPUT = ROOT / "main" / "assets"
SIZE = (240, 320)

ASSETS = {
    "rock": SOURCE / "rock-generated-v2.png",
    "scissors": SOURCE / "scissors-generated-v2.png",
    "paper": SOURCE / "paper-generated.png",
}


def crop_cover(im: Image.Image, size: tuple[int, int]) -> Image.Image:
    target_ratio = size[0] / size[1]
    ratio = im.width / im.height
    if ratio > target_ratio:
        width = round(im.height * target_ratio)
        left = (im.width - width) // 2
        im = im.crop((left, 0, left + width, im.height))
    else:
        height = round(im.width / target_ratio)
        top = (im.height - height) // 2
        im = im.crop((0, top, im.width, top + height))
    return im.resize(size, Image.Resampling.NEAREST)


def rgb565_le(im: Image.Image) -> bytes:
    out = bytearray()
    for r, g, b in im.convert("RGB").get_flattened_data():
        pixel = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        out.extend(struct.pack("<H", pixel))
    return bytes(out)


def main() -> None:
    PREVIEW_OUTPUT.mkdir(parents=True, exist_ok=True)
    RAW_OUTPUT.mkdir(parents=True, exist_ok=True)
    for name, source in ASSETS.items():
        image = crop_cover(Image.open(source).convert("RGB"), SIZE)
        image = ImageEnhance.Contrast(image).enhance(1.08)
        image.save(PREVIEW_OUTPUT / f"{name}-preview.png", optimize=True)
        raw = rgb565_le(image)
        assert len(raw) == SIZE[0] * SIZE[1] * 2
        (RAW_OUTPUT / f"{name}.rgb565").write_bytes(raw)
        print(f"{name}: {source.name} -> {len(raw)} bytes")


if __name__ == "__main__":
    main()
