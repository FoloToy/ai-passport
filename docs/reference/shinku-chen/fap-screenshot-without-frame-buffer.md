<p align="right">
  <a href="fap-screenshot-without-frame-buffer.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Answering `FAP_SCREENSHOT_V1` Without a Spare Frame Buffer

This reference originated from the **limelight lemonade jam reader** (`v0.1.0-limelight`,
commit `dc7154d`) on a no-PSRAM AI Passport. The measurements below were taken on that
board; the firmware was accepted by the community publisher, whose own capture tool
produced the PNG and receipt used for the submission.

## Why the protocol has to be answered

The community publisher requires a recent serial capture as gameplay evidence, and
rejects firmware that only prints logs. Its tool opens the port at 115200 baud, writes
`FAP_SCREENSHOT_V1\n`, then expects one ASCII header followed immediately by the
declared payload:

```text
FAP_SCREENSHOT_V1 <width> <height> RGB565LE|PNG <byte_length>\n
<binary payload>
```

Three rules from the tool shape the implementation: it reads **exactly** `byte_length`
bytes, it gives the whole exchange a **15 second** budget, and a valid capture writes a
`.fap-capture.json` receipt that `validate` and `submit` accept for 24 hours.

## The constraint: there is no spare 150 KB

The obvious implementation is one static 240 × 320 × 2 = 153,600 byte frame buffer plus
`lv_snapshot_take_to_draw_buf()`. On this board that buffer cannot be had: the reader
already keeps a 240 × 214 art canvas (102,720 B), a 40 KB sprite decode buffer, a
20.5 KB script cache, a 3.2 KB mask buffer and a 28 KB LVGL pool, and the largest free
block at boot is about 9 KB.

The way out is to reuse the art canvas and capture in **two passes**, because the
protocol only constrains the byte order of the stream, not how the sender produces it:

1. Announce `240 320 RGB565LE 153600`.
2. Capture rows `[0, 214)` into the canvas buffer and write those 102,720 bytes.
3. Capture rows `[214, 320)` into the same buffer (a second pass that folds the rows
   down to the buffer start) and write the remaining 50,880 bytes.

Total: the exact declared length, no second buffer, and the existing frame-capture path
(an LVGL flush hook that copies flushed rows into the canvas) does the work.

## Four traps that cost real time

**The capture buffer is also the on-screen canvas.** Clearing it before a capture — which
is necessary, because LVGL refreshes in partial areas and stale rows otherwise survive —
blanks the backgrounds and sprites, both in the returned frame *and* on the panel. The
fix is to redraw the art immediately after clearing, before invalidating and refreshing:

```c
memset(s_art_pixels, 0, sizeof(s_art_pixels));
lime_app_debug_redraw_art(&s_app);   /* forces a fresh background/sprite blit */
s_capture_row_offset = row_offset;
s_capture_frame = true;
lv_obj_invalidate(lv_screen_active());
lv_refr_now(NULL);
```

**The default console path is far too slow.** Writing through the register-level
USB-Serial-JTAG VFS measured **2.7 KB/s**: a full frame took **56 seconds**, so the
publisher's 15 second budget expired and it reported "device screenshot data ended
before the declared length". Installing the driver and switching the VFS to it took the
same transfer to **1.66 seconds**:

```c
usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
cfg.tx_buffer_size = 1024;   /* the reference implementation's size is enough */
cfg.rx_buffer_size = 256;
usb_serial_jtag_driver_install(&cfg);
usb_serial_jtag_vfs_use_driver();
```

**Short writes silently drop whole chunks.** With `fwrite()` the transfer lost exactly
one 2048-byte chunk in a run, which shifts every following pixel and still looks like a
plausible image. Check the return value and retry; writing through
`usb_serial_jtag_write_bytes()` in 512-byte pieces with a stall counter is enough.

**A log byte in the payload shifts the whole image.** The console and the payload share
one USB-CDC stream, and the host reads an exact byte count, so any `ESP_LOG` line that
lands mid-transfer corrupts the frame. Silence logging for the duration
(`esp_log_level_set("*", ESP_LOG_NONE)`) and restore it afterwards.

## Two placement rules for the driver

- **Do not install it at the top of `app_main()`.** Done there, the board booted with no
  output at all and had to be re-flashed; the VFS must already be set up. Install it
  after the application is initialised but **before** the task that reads commands from
  the VFS starts.
- **Create large task stacks before installing it.** The driver buffers come out of the
  same heap: creating an 8 KB-stack input task after the driver failed with an "input
  task creation failed" error and left the reader dead, even though 17 KB were still
  free. Creating the task first, then installing the driver with 1 KB/256 B buffers,
  boots with 1.8 KB of free heap.

## Verifying a capture

`capture-screen --port <port> --output capture.png` writes the PNG and its receipt. If
the tool reports a short payload, measure the firmware side first: a small host script
that speaks the protocol prints how many bytes arrived and how long they took, which
separates a slow path (tens of seconds) from a truncating one.
