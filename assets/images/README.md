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

`senior-safety-card-cover.png` is a 240 × 320 (3:4) community-publishing cover
captured directly from the physical device's LVGL framebuffer over USB. It is
the verified first information page, not a camera photo or a hardware mockup.
The selected page contains no phone number, QR payload, credential, or other
private contact data. The image is a publishing asset and is not compiled into
firmware.
