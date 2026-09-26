<p align="right">
  <a href="lvgl-flush-path-frame-capture.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Capturing the Real Screen from the LVGL Flush Path

Captured while publishing the *Starry Sky Railroad and Shiro's Journey* reader.
A visual-novel port has to be checked page by page — text band, name plate,
sprite placement, chapter label, battery — and a phone photo of a 240 × 320
panel is a poor substitute for the pixels. This board has no PSRAM and the UI
already owns one full-screen canvas for the art, so a second 150 KB framebuffer
for screenshots is not affordable. The solution is to capture the frame that is
already being composited: hook the display's flush, copy every tile into the art
canvas, and stream it out over the console. Zero extra RAM, and the result is what
the eye sees (the BSP's rounded-corner masking runs in an earlier flush callback,
so the capture includes it).

## How the capture works

1. Register a display `LV_EVENT_FLUSH_START` callback **after** the board's own
   mask callback, so it sees the already-masked, not-yet-byte-swapped pixels.
   `lv_display_get_buf_active(disp)` plus the event's `area` give the tile and its
   stride; copy the rows into the canvas at the same coordinates.
2. Enable the copy only for the capture: set a flag, call
   `lv_obj_invalidate(lv_screen_active())` and `lv_refr_now(NULL)` for a full
   redraw, then clear the flag. With a 40-line partial buffer the whole screen
   arrives as eight tiles; copying a tile only after it was rendered keeps the rest
   of the canvas intact, so the capture is self-consistent.
3. Stream the canvas as native little-endian RGB565 (240 × 320 × 2 = 153,600
   bytes) with the console's LF→CRLF translation disabled around the binary
   payload, then restore it.

## The traps

- **`lv_snapshot` is the wrong tool for a canvas-backed UI.** `lv_snapshot_take_*`
  clears the destination buffer when it cannot find a top object and then renders
  the widget tree into it. Pointed at the art canvas, the first capture came back
  as widgets over black — the background had been erased. Rendering with the canvas
  visible instead makes the canvas copy itself after the same clear, which is no
  better. Capture the flush path and leave the snapshot API out of it.
- **Hold the LVGL lock for the whole transfer.** At 115200 baud the payload takes
  about 13 s. Releasing the lock between capture and transfer produced a torn
  image: a keypress re-decoded the background into the very buffer being read out
  (the capture showed a doubled sprite and noise bands). Holding the lock freezes
  the UI for the transfer, which is the correct trade for a diagnostic command.
- **The task doing the capture needs a real stack.** A full re-render inside a
  4 KB debug task hit `Stack protection fault` and rebooted the board; 8 KB is
  enough for the same work. The failure looked like a device crash unrelated to
  screenshots until the faulting task name was read.
- **Byte order is decided by where you hook.** The port swaps RGB565 bytes before
  SPI transfer, so the copy must run before that (a `FLUSH_START` callback is).
  Capturing after a swap would need an un-swap on the host.
- **A full re-render is the price.** One capture re-decodes the background JPEG
  and re-composites the frame (130–170 ms here); the command also leaves the
  canvas holding the captured frame, so re-render the scene afterwards to keep the
  device state clean.

## Choosing between this and a reserved buffer

[y2lin's serial screenshot entry](../y2lin/serial-screenshot-protocol.md) reserves
a static full-screen buffer and answers `FAP_SCREENSHOT_V1`. That is the right
shape when the UI does not already own a full-screen buffer and the budget allows
one. When it does — a canvas that holds the whole screen anyway — reusing it costs
nothing and the capture cannot drift from what is on the panel.

## Related

- [`release-artifact-verification.md`](release-artifact-verification.md) — reading
  the published image's startup log is the other cheap way to check a release on
  hardware.
- [`display-refresh-and-deep-sleep.md`](display-refresh-and-deep-sleep.md) — the
  panel refresh path this hook attaches to.
