<p align="right">
  <a href="serial-screenshot-protocol.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Implementing the FAP_SCREENSHOT_V1 Serial Screenshot Protocol

Captured after publishing the **Sound Sentry** ambient sound meter to the AI
Passport community market. The publisher workflow requires a fresh device
screenshot captured over the `FAP_SCREENSHOT_V1` serial protocol, and this
entry records the pitfalls hit while implementing that protocol on ESP32-C3
with LVGL 9.5 (commits `6acfd9d` through `cc1d72f`).

The protocol itself is small: the host sends the ASCII line
`FAP_SCREENSHOT_V1\n` over USB-CDC, and the device replies with the header
`FAP_SCREENSHOT_V1 <width> <height> RGB565LE <bytes>\n` followed by exactly
`<bytes>` little-endian RGB565 pixels. Every trap below is in the device-side
plumbing, not in the protocol.

## Install the USB-serial-JTAG driver before reading anything

A single-app firmware that boots straight into its UI starts no REPL, and
console logging goes through the register-level VFS path, so nothing in the
system ever installs the USB-serial-JTAG driver. Calling
`usb_serial_jtag_read_bytes()` then dereferences a NULL driver object — on
this board it crashed in a loop, and because the backlight was already on, the
visible symptom was "the screen keeps flickering", not a serial error. Install
the driver explicitly (modest buffers suffice: 256-byte rx, 1024-byte tx),
call `usb_serial_jtag_vfs_use_driver()` to move the console onto the driver
path, and have the reader task poll `usb_serial_jtag_is_driver_installed()`
at a low frequency instead of assuming readiness.

## Never busy-loop a serial reader task

A persistent error return from `usb_serial_jtag_read_bytes()` spins a tight
read loop, starves the idle task, and trips the task watchdog — the device
reboots in a loop and again *looks* like a flickering screen. Back off (200 ms)
on errors, drop the task below the LVGL priority (LVGL runs at 4; a serial
helper at 3), and give every nothing-to-do path a `vTaskDelay()`. On this
board the flicker had two independent causes (the NULL driver and the
busy-loop); fix both classes, not just one.

## Match the command as a substring, not a terminated line

Windows terminal tools can swallow the trailing newline, so a strict
newline-terminated line match never fires and the device stays silent while
the host times out. Match a sliding window of the last command-length bytes
against the ASCII command, reset the window on any line terminator, and clear
it after a match so residual bytes cannot re-trigger the capture.

## Render the snapshot into a statically reserved buffer

Two LVGL 9.5 facts come first: the snapshot helpers stay disabled unless
`CONFIG_LV_USE_SNAPSHOT=y` is set (an implicit-declaration build error is the
first symptom), and `lv_snapshot_take()` returns an `lv_draw_buf_t*` — the
`lv_image_dsc_t*` pattern from older examples no longer compiles.

The heap is the real trap. With roughly 220 KB free, the runtime heap still
could not produce one contiguous 153,600-byte block (240x320x2) once LVGL,
the codec stack, and DMA had been running; the dynamic allocation failed on
every request while the free-heap log looked healthy. Reserve the full-screen
RGB565 buffer at compile time (64-byte aligned), initialize it with
`lv_draw_buf_init()`, and render with `lv_snapshot_take_to_draw_buf()` — the
official external-buffer pattern that never touches the LVGL heap. Hold the
LVGL lock only for the render instant (`bsp_lvgl_lock()` /
`bsp_lvgl_unlock()`), so the UI keeps animating during the transfer, and give
the task stack headroom: full-screen software rendering was comfortable with
8192 bytes.

## Stream the payload in chunks sized to the tx ring buffer

`usb_serial_jtag_write_bytes()` bottoms out in `xRingbufferSend()`, which
rejects any item larger than the ring buffer capacity outright ("data will
never ever fit"). Submitting the 153,600-byte payload in a single call sends
nothing at all, and the host reports that the data ended before the declared
length. Stream the payload in 512-byte chunks (well under the 1024-byte tx
buffer), blocking per chunk with a generous timeout, and abandon the transfer
— but not the task — when a chunk fails because the host unplugged.

## Mute logs during the binary window

ESP-IDF log lines share the same USB-CDC stream as the reply, and the host
reads exactly the declared byte count, so a single interleaved log byte shifts
the whole image. Set `esp_log_level_set("*", ESP_LOG_NONE)` immediately before
the first header byte, restore the default level after the last pixel byte,
and keep any post-transfer logging strictly outside that window.

## Keep the command read-only and fail silently

The publisher's contract is observational: the command never reboots, flashes,
or changes settings. On any failure (lock timeout, snapshot mismatch, driver
gone), log the reason and send nothing — the host surfaces a clean timeout
error instead of a corrupted stream. Defensively verify that the snapshot is
a tight-packed full-screen RGB565 image before answering at all.

## Generalization for the next app

- Anything that reads the USB-serial console must first make sure the driver
  is installed; booting without a REPL leaves that job to the app.
- Debug the pipe with a raw serial terminal (local echo on) before debugging
  the protocol — it distinguishes "command never arrived" from "device did
  not answer".
- Size serial payload writes to the tx ring buffer, not to the payload.
- Reserve full-screen frame buffers statically; runtime fragmentation makes
  contiguous 150 KB allocations unreliable on a no-PSRAM part.
- Keep binary replies and log output mutually exclusive in time.
- Treat a flickering screen as a crash-loop symptom and read the serial log
  before touching display code.

## Related documents

- `docs/development/publish-to-community.md` — the publishing workflow that needs this screenshot.
- `docs/experiences/shinku-chen/post-release-follow-up.md` — the release-side view of the same flow.
- `main/fap_screenshot.c` — the implementation the commits above landed.
- `docs/CHANGELOG.md` — the Unreleased entry describing the shipped protocol.
