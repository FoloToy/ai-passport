<p align="right">
  <a href="live-overlays-and-audio-on-a-handheld.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Live Overlays, Redraws and Audio on a No-FPU Handheld

Also captured from **Dust of Worlds** `v0.27` (community revision `5371`). The
application keeps a map, four pages, a random-event overlay and a small sound
engine running together on an ESP32-C3 with no FPU, no PSRAM and a 240 × 320
panel. Three classes of problem were worth writing down: a full-screen effect
that starved everything else, an audio path that was both harsh and stuttering,
and text that silently lost glyphs after copy changes.

## A full-screen opacity animation is a full-screen redraw per frame

The first divine-power effect faded a full-screen colour overlay from opaque to
transparent over about 700 ms. It looked reasonable in a preview and was a
disaster on the device: every animation frame invalidated the whole screen, the
LVGL task spent the frame blending, and the audio task could not refill the I2S
DMA in time. The effect and the sound both stuttered.

The replacement was deliberately boring:

- show a static overlay (or a small banner) with the result text;
- leave it visible for a fixed time (about 4.5 s) and hide it from the main
  update loop when the deadline passes;
- do not pause the world, do not capture the keys, and do not animate opacity.

The effect is still legible, but it costs two small invalidations instead of
dozens of full-screen ones. If a full-screen flash is genuinely wanted, keep it
to one or two frames; never animate it over hundreds of milliseconds on this
class of hardware.

Two supporting habits:

- redraw the map only when the simulation tick changed or a dirty flag is set,
  not on every UI update;
- hold the LVGL lock for the whole update and keep the work per update bounded;
  log the LVGL pool and the system heap at boot and after the largest page is
  built.

## The audio path: 16 kHz, soft waveforms, and a task that is not starved

The first sound engine used square waves at 22.05 kHz and played them from a low
priority task. On this board that was both harsh (square harmonics) and unstable
(the codec/I2S path was configured around 16 kHz and the task lost the CPU during
full-screen redraws). The fix had four parts:

1. **16 kHz**, matching the BSP's default audio configuration, instead of 22.05 kHz.
2. **Sine plus a quiet second harmonic** for tones, not a square wave; low-passed
   noise for fire, flood and rumble; a short attack and a longer release envelope
   on every sound to remove clicks and the piercing edge.
3. **1024-sample chunks** written from a dedicated task at a priority above the
   LVGL task (6 in this build), so a page redraw cannot starve the DMA.
4. **Amplitude discipline**: keep the synthesized peak well below full scale and
   clamp the stored volume (70 here) so a user setting cannot make every cue
   painful.

Six divine events got distinct short phrases (descending chimes for ice, a low
swell for flood, a major arpeggio for blessing, and so on) rather than one beep
with a different pitch. The same engine plays key clicks, confirms, back, sleep
and wake cues, so the audio task runs often; keeping each cue short and the queue
shallow avoids delayed or overlapping sounds.

## Glyph coverage is a build artifact, not a runtime hope

The UI language is Chinese and the font is a 4 bpp subset generated from the
source strings. After a few releases of new event text, the chronicle started
showing placeholder boxes: the new characters had never been added to the font.
The code was correct; the artifact was stale.

Two habits prevent a repeat:

- regenerate the font as part of every release that changes copy, before the
  firmware build;
- keep a static coverage check that scans the application's own strings (and
  comments, which the generator also consumes) and compares them with the
  generated cmap. A missing glyph should fail the build, not appear as a box on
  the device.

The font generator also needs to be deterministic about the format (LVGL 9,
4 bpp, the same line height); a mismatch between the generated font and the
label layout shows up as clipped or overlapping lines rather than as an error.

## Header layout: do not pack unrelated numbers into one label

The top bar originally held era, year and population in a single 188 px label.
That works until the population reaches millions: the text is clipped on the
right and the user sees an incomplete number. Splitting it into two labels —
era plus year on the left, population right-aligned in its own box — and moving
the speed indicator to a small dot removed the class of bug entirely. The same
review should be done for the largest plausible string (for example 1.2 trillion),
not for the starting value.

The general rule: if two numbers grow independently, give them independent
labels and independent alignment.

## Takeaways

- Never animate a full-screen opacity on this hardware; use a static overlay with
  a deadline and keep the world running.
- Redraw on simulation change, not on a timer that fires faster than the content.
- 16 kHz, soft waveforms, envelopes and a high-priority audio task; keep chunks
  large enough to survive a redraw.
- Treat the font as a build artifact: regenerate it with the copy and gate glyph
  coverage in the build.
- Split independently growing numbers into separate labels and test the largest
  string, not the smallest.
