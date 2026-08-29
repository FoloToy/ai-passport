<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Images

Store reusable source images and generated display assets here.

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

## Senior Safety Card cover

`senior-safety-card-cover.png` is a 1152 × 1536 (3:4) community-publishing
cover. It was generated with the built-in image generation tool from the
official `docs/assets/brand/ai-passport-front.png` hardware reference. The
prompt required the external hardware to remain unchanged and replaced only the
screen with the implemented pixel-style safety-card page. The image is a
publishing asset and is not compiled into firmware.
