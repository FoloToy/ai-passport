<p align="right">
  <a href="ble-active-idle-power.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Idle Power Behind an Always-On BLE Link

Experience from the Pocket Intercom: the phone link is the product, so the device has to
lower its own draw while idle without ever losing that link. The useful steps turned out
to be ordinary DFS, a codec suspend, and one non-obvious activity rule - and the last of
them was the difference between "power saving enabled" and "power saving that ever runs".

## Do not enable system light sleep while the link is up

`CONFIG_PM_ENABLE` with dynamic frequency scaling is safe: the CPU drops to its 40 MHz
floor between work items and the link stays up. Enabling light sleep through the
automatic power-management path - `light_sleep_enable = true` in `esp_pm_configure()` -
does not: with an active BLE connection the link dropped and the phone reported a GATT
supervision timeout (`status=8`), then reconnected. Note this automatic entry is not how
the upstream Low Power demo enters light sleep; it calls `esp_light_sleep_start()`
explicitly. The observed behavior was consistent and reproducible, so the product uses
DFS only. Keep the controller's own modem sleep (`CONFIG_BT_CTRL_MODEM_SLEEP`) enabled;
that one does not stall the link.

The practical rule for this class of device: any power state that can delay the radio's
supervision response is off limits while the link is the product.

## Count only real traffic as activity

The phone sends a 1 Hz control frame for as long as the app is connected. The first
implementation reset the idle timer on any received BLE packet, so the device never saw
itself as idle and never entered the low-power state - the saving existed in the log
messages only. Activity now counts only the frames that carry something the user can
perceive: downlink audio and on-screen text. After that the idle transition fired on
schedule and stayed stable.

## Suspend the audio codec while idle, wake it on activity

After the idle delay the firmware runs the ES8311 suspend sequence verified in
[Shutting Down On-Board Peripherals Before Deep-Sleep](deep-sleep-peripheral-power-off.md)
and logs the readback result; the first real activity wakes the codec again. The wake path
has to run *before* the first frame of a new sentence needs to play, not when the frame
arrives. Verified over repeated cycles: every round played out with frames decoded equal
to frames received, nothing dropped and nothing left over.

## Turning the backlight off is not the same as turning the panel off

A brightness of 0 only hides the picture: the controller keeps scanning its frame memory into
the glass, and that scan - not the memory itself - is what costs current while the device is
otherwise idle. `esp_lcd_panel_disp_on_off(panel, false)` (DISPOFF) stops the refresh and
leaves the frame memory intact, so the idle path can blank the panel and the wake path can
restore the screen immediately instead of repainting it.

Two details are worth copying:

- **Keep the reversible path separate from the terminal one.** Runtime blanking uses only
  DISPOFF/DISPON. SLEEP IN stays in the deep-sleep preparation helper, which is a one-way
  door (it also stops the backlight PWM, and the device is about to reboot anyway). DISPOFF
  costs nothing to undo; SLEEP IN needs a ~120 ms SLPOUT sequence, and whether the controller
  carries the frame memory through it is not something the datasheet states clearly enough to
  rely on in a product path.
- **Order the wake correctly: panel first, backlight second.** Turning the backlight back on
  while the panel is still off shows one frame of not-quite-ready content.

Holding the image is the cheap half of the trade, so an instant wake is essentially free. Both
directions were then verified on hardware: the screen blanks when the idle timeout expires,
and a key press brings the same content back instantly.
As always on this board, measure the real draw with an instrument before quoting numbers;
the battery gauge reports state of charge and voltage only.

## Deep sleep is a separate, later step

Deep sleep only makes sense once the earlier steps hold, and it needs the key-pad hand-off
described in
[Landscape Rotation and a Deep-sleep Key Wake](landscape-rotation-and-deep-sleep-key-wake.md)
(an ADC-owned pin otherwise makes a low-level key wake fire at sleep entry). Two rules kept
it non-flapping:

- Refuse to sleep while a key is held, and retry later instead of entering and immediately
  leaving sleep in a loop.
- Verify the hand-off by logging the pin level after the transfer; "the code ran" is not
  evidence that the pin reads the right level.

## Measure from the link, not from the serial log

A device in deep sleep powers down its USB serial port, so the capture goes silent by
design. Worse, after any device reboot the USB port re-enumerates and an already-running
capture quietly stops receiving data while still looking healthy. Both facts make "no log
lines" a bad signal. The peer's connection stream (connect / disconnect events) is the
reliable way to answer "did it really stay asleep, and did it come back".

## Reconcile with the device's own counters

The playback path prints counters for every utterance: frames received, frames decoded,
frames dropped, frames still queued at the end. A non-zero "still queued" value at stream
end is what exposed a real bug - the sentence was being declared finished on an idle
timeout while frames were still waiting in the decode queue, so long replies were cut
short. The device reporting its own numbers is what turned a vague "audio sometimes stops"
report into a reproducible defect with a fix.

## Check this collection before instrumenting

Everything above was eventually answered by material that already existed here or in a
branch name; the time went into re-deriving it on hardware. When a question sounds like
"how should this be done on this board", search this directory, the project README and the
branch list first.
