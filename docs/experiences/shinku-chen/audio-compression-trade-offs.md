<p align="right">
  <a href="audio-compression-trade-offs.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Audio Compression Trade-offs on ESP32-C3

Captured after the **Voice Keychain** release (commit `91466b0`). This entry records
how the audio codec for a voice-playback application was chosen on ESP32-C3 (8 MB
flash, no PSRAM), so the next developer who needs to fit a large voice library into
a bounded flash partition can start from measured numbers instead of estimates.

## Context and the hard limit

The application plays a library of Chinese voice clips stored in a dedicated SPIFFS
data partition. The source asset set is large:

- 38 directories / packs, 1557 clips, ~4598 s of audio.
- Encoded at the original bit rate, the full set is ~35.1 MB.

The target is a data partition the firmware must also be able to decode. On this
hardware the partition size and the codec together set how much of the library is
actually playable. This is a physics ceiling: no amount of selection recovers the
bulk of the content once a codec is fixed.

## Methods compared

| Method | Bytes/second (effective) | Decoder cost on ESP32-C3 (no PSRAM) |
| --- | --- | --- |
| IMA-ADPCM 4-bit | ~7800 B/s (16 kHz mono) | Already present, negligible CPU/RAM |
| MP3 | nominal 4000–8000 B/s | ~30–40 KB flash decoder, moderate CPU |
| Opus | ~1000 B/s (6 kbps), ~1500 B/s (12 kbps) | ~60–80 KB flash + some RAM, moderate CPU |

Measured capacity of the current set (what actually fits in a 3 MB partition):

| Method / bit rate | Share of the full set that fits |
| --- | --- |
| IMA-ADPCM 4-bit | ~8.7% (~402 s) |
| Opus 12 kbps | ~46.7% (~2146 s) |
| Opus 6 kbps | ~66.8% (~3072 s) |

The decisive counterweight is the decoder: an Opus decoder costs ~60–80 KB of flash
and some RAM on a chip with no PSRAM, which must be budgeted against the application
and the LVGL UI. The right choice depends on whether raw capacity or firmware
simplicity matters more.

## Decision and what was built

The published app **migrated to Opus 8 kbps** (the current high capacity at low bit
rate) with a `libopus` component decoder, over a raw-packet stream where each packet
is a 2-byte little-endian length plus one Opus frame. The encoder (`encode_opus.py`)
resamples to 16 kHz mono, and the firmware decodes frames in a dedicated task and
writes PCM to the audio output.

A full tool was not needed in the final build: the previous IMA-ADPCM encoder
(`encode_voice.py`) and its legacy comparison document remain in the fork as the
recorded decision history, and the Opus path is the shipped implementation.

The analysis above is against the 3 MB (`0x300000`) partition that was in place
at the time. The final shipped configuration keeps the Opus decoder and uses a
`0x210000, 0x5F0000` (~5.94 MB) `voicefs` partition, so the playable capacity
exceeds the 3 MB numbers used here.

## Detailed measurement

The full measurement that informed the choice, so the numbers above can be
taken as data rather than claims.

### Scope and measurement basis

- Target: ESP32-C3, 8 MB flash, no PSRAM, ESP-IDF 5.5.3.
- Data partition: `voicefs`, 3 MB (`0x300000`) at the time of this analysis,
  SPIFFS, mounted at `/voices`. (The final shipped partition is `0x210000,
  0x5F0000`, ~5.94 MB; see the decision below.)
- Source set measured: the current `assets/project` (38 directories, 1557 clips,
  ~4598 s of audio). All clips are mp3/ogg/wav; they are decoded, low-passed,
  silence-trimmed, then encoded.
- The firmware decodes IMA-ADPCM 4-bit in software (`main/voice_app.c`); any
  other decoder must be added to the firmware.

### Why the size is a hard limit

IMA-ADPCM 4-bit at 16 kHz mono stores 4 bits per sample: `16000 × 4 / 8 = 8000
bytes/s`. With silence trimming the measured rate over the current set is
~7800–8000 B/s. So with the current codec, the 3 MB partition holds about 8.7%
of the full current asset set. This is the baseline.

### Per-method detail

- **IMA-ADPCM 4-bit (current)**: fixed 8000 B/s (4 bits/sample × 16 kHz); decoder
  already in `main/voice_app.c`, tiny, negligible CPU/RAM; intelligible, simple,
  deterministic, no extra library.
- **MP3 (potential)**: selectable 32–64 kbps (64 kbps = 8000 B/s, 32 kbps = 4000
  B/s); no decoder in ESP-IDF by default, needs an MP3 library (Helix, minimp3,
  or the ESP-ADF MP3 component), ~30–40 KB flash plus RAM and CPU; more
  CPU-intensive than IMA-ADPCM.
- **Opus (potential)**: selectable 6–24 kbps (12 kbps = 1500 B/s, 24 kbps = 3000
  B/s); no built-in decoder, port libopus or a component; ~60–80 KB flash and
  some RAM on no-PSRAM, moderate CPU; far more efficient than IMA-ADPCM at low
  bit rates for speech.
- **Raw PCM (baseline, not used)**: 16 kHz mono 16-bit = 32000 B/s, ~4× larger
  than IMA-ADPCM; not used in this product.

### Measured results

The OPUS numbers were measured by real encoding with ffmpeg 4.4 (libopus) over
clips from the current set at three rate points. Effective bytes/second differ
from the nominal kbps because per-segment container/frame overhead adds a little.

| Sample | Duration | Opus 6 kbps | Opus 12 kbps | Opus 24 kbps |
| --- | --- | --- | --- | --- |
| cxk (short) | 0.61 s | 867 B/s | 1593 B/s | 2871 B/s |
| mama (mid) | 2.55 s | 1084 B/s | 1623 B/s | 4284 B/s |
| ren sheng (long) | 14.02 s | 845 B/s | 1532 B/s | 3216 B/s |

Effective rates: **6 kbps ≈ 850–1000 B/s, 12 kbps ≈ 1500–1600 B/s, 24 kbps ≈
2900–4300 B/s.**

### Capacity at 3 MB

Percent of the full current set that fits in the 3 MB partition, by method and
bit rate. Higher is better, but the firmware decoder cost is the counterweight.
The IMA-ADPCM rows are measured over the current set; the OPUS rows use the
measured rates; the MP3 rows are the nominal rate (no MP3 measurement run).

| Method / bit rate | B/s | Fits in 3 MB | Share of set |
| --- | --- | --- | --- |
| IMA-ADPCM 4-bit | 7800 | ~402 s | ~8.7% |
| MP3 64 kbps (nominal) | 8000 | ~402 s | ~8.7% |
| MP3 32 kbps (nominal) | 4000 | ~805 s | ~17.5% |
| Opus 24 kbps | 4300 | ~1048 s | ~22.8% |
| Opus 12 kbps | 1500 | ~2146 s | ~46.7% |
| Opus 6 kbps | 1000 | ~3072 s | ~66.8% |

Even by the most efficient packing, the 3 MB partition holds only 8 of 38
directories under IMA-ADPCM, covering ~9.4% of total duration. The largest, most
popular packs (each exceeding a third of the partition) are the first dropped:
Jile (4385 KB, 570 s) alone exceeds the whole partition; Kenan (3172 KB, 413 s);
Hajimi (3089 KB, 398 s); JoJo (2505 KB, 323 s).

### Firmware decoder cost

The decisive counterweight to raw compression ratio.

| Method | Decoder needed | Flash (est.) | RAM (est.) | CPU |
| --- | --- | --- | --- | --- |
| IMA-ADPCM | already present | ~0 (already built) | negligible | very low |
| MP3 | add component | ~30–40 KB | small | moderate |
| Opus | add libopus | ~60–80 KB | moderate | moderate |

### Recommendation summary

Two viable paths, in order of least risk:

1. **Keep IMA-ADPCM, enlarge the partition.** The 8 MB flash has ~1.94 MB
   unallocated; growing `voicefs` from 3 MB to ~5 MB raises the IMA-ADPCM ceiling
   to ~13.7%. No new decoder, minimal firmware change, but still only about a
   seventh of the set.
2. **Add a small Opus decoder and keep 3 MB.** At 12 kbps this recovers nearly half
   the set for the same partition footprint, but requires porting a decoder and
   re-validating decode CPU/RAM on the device.

If the product goal is "make as much of the current set playable as possible,"
path 2 (Opus) delivers the most content per flash; if the goal is "no new firmware
risk," path 1 (enlarge the partition with the existing codec) is the safer
increment. Either way, the current asset set cannot fully fit in 3 MB without a
codec change.

## Reusable takeaways

- **Measure, don't estimate.** The effective bytes/second per clip differs from the
  nominal bit rate because of per-frame/container overhead. Real encoding of the
  actual asset set (not a synthetic sample) is what determines whether the library
  fits.
- **Capacity is fixed by the codec, not by selection.** Once the codec and partition
  size are set, the playable fraction is a ceiling; trimming the library cannot
  recover content that exceeds the partition.
- **Decoder cost is the real trade-off on no-PSRAM.** Raw compression ratio alone is
  misleading; the extra flash and RAM for an Opus/MP3 decoder has to be budgeted
  against the UI and the application.
- **Keep the partition-mount footnote.** Using SPIFFS (an ESP-IDF built-in
  component) avoids pulling an external Managed Component, and `format_if_mount_failed =
  true` lets a blank data partition self-format on first boot, simplifying first-run
  flashing.

## Other reusable implementation notes

These are concrete, useful details from the same release, worth recording for any
developer building a Chinese-voice UI or debugging display/input on this board.

- **Bump the mic gain when recognition is weak.** If the microphone picks up speech
  poorly, raise the input gain at the codec driver rather than adding a software
  preamp — `esp_codec_dev_set_in_gain(s_dev, 30.0f)`.
- **Match `BSP_LCD_H` with the UI height.** The screen is an ST7789P3 `240x320`
  (`BSP_LCD_W=240`, `BSP_LCD_H=320`). When drawing a full-height canvas, the UI
  height (`UI_H`) must equal `BSP_LCD_H`; if a white band appears at the edge,
  re-check that the canvas height and the panel height agree.
- **Capture the screen for remote debugging.** Enabling
  `CONFIG_LV_USE_SNAPSHOT=y` gives a whole-frame snapshot without extra
  dependencies. Alternatively the board can be driven through ADB so an agent can
  screenshot and recognize the display.
- **Tune input timing to responsiveness.** The short-press and long-press thresholds
  are set in the button component (`config->long_press_time`, `config->
  short_press_time`). A long-press duration around 500 ms and a short-press around
  180 ms feel more responsive than the defaults; the long-press-repeat interval is
  set by the component Kconfig. Note the code drives `long_press_time` directly,
  bypassing the Kconfig 500 ms lower bound, so tune it in code.
- **Embed a CJK subset font for Chinese UI.** Generate a Noto Sans CJK SC subset
  with `lv_font_conv` and wire it via `LV_LVGL_H_INCLUDE_SIMPLE=1` so the font uses
  a plain `<lvgl.h>` include instead of a `lvgl/`-prefixed path. Non-BMP characters
  (for example the `🐦`) have no glyph in the subset, so the transcoder replaces
  them with readable Chinese before building.
- **Reuse the encoder pipeline.** The Opus encoder (`encode_opus.py`) imports the
  IMA-ADPCM encoder (`encode_voice.py`) as `EV` and reuses its display-name
  cleaning and C index generation, so the two parallel encode paths share one
  index generator instead of drifting.
- **Keep button-count ownership in one place.** The button component reads the
  count from the pin table (`BSP_BTN_COUNT` in `bsp_pins.h`) rather than
  re-declaring it, avoiding two near-identical `BSP_BTN_COUNT` symbols. The three
  physical buttons share one ADC pin and are distinguished by a divider resistor;
  after changing dividers, measure the three voltage windows with
  `bsp_button_read_mv()` before updating `BSP_BTN_MV_TABLE`.

## Route

This is general, upstream-benefiting hardware and design experience, so it is
proposed back to the upstream `FoloToy/ai-passport` as a documentation PR.
