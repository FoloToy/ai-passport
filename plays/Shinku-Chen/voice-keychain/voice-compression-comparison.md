<p align="right">
  <a href="voice-compression-comparison.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Voice Compression Methods on ESP32-C3

This document compares the compression methods available for the **Voice Keychain** data partition, with **measured** numbers taken from the current `assets/project` set, so the trade-off between "how much audio fits" and "what the firmware must decode" is grounded in real numbers rather than estimates.

## Scope and measurement basis

- Target: ESP32-C3, 8 MB flash, no PSRAM, ESP-IDF 5.5.3.
- Data partition: `voicefs`, 3 MB (`0x300000`), SPIFFS, mounted at `/voices`.
- Source set measured: the current `assets/project` (38 directories, 1557 clips, ~4598 s of audio). All clips are mp3/ogg/wav; they are decoded, low-passed, silence-trimmed, then encoded.
- The firmware today decodes **IMA-ADPCM 4-bit** in software (`main/voice_app.c`). Any other decoder must be added to the firmware.

## Why the size is a hard limit

IMA-ADPCM 4-bit at 16 kHz mono stores 4 bits per sample: `16000 × 4 / 8 = 8000 bytes/s`. With silence trimming the measured rate over the current set is ~7800–8000 B/s. That gives the ceiling below.

| Item | Value |
| --- | --- |
| Full current set, encoded | ~35.1 MB |
| Total audio duration | 4598 s |
| Measured bytes/second | ~7800 B/s |
| 3 MB partition capacity | ~402 s (~8.7% of the set) |

So with the current codec, the 3 MB partition holds about 8.7% of the full current asset set. This is the baseline.

## Method comparison

The two axes that matter are (a) compressed bytes per second of audio, and (b) the decoder the firmware must carry. OPUS is not free: it requires adding a decoder with flash and RAM cost on a chip with no PSRAM.

### IMA-ADPCM 4-bit (current)

- Bit rate: fixed 8000 B/s (4 bits/sample × 16 kHz).
- Decoder: already in `main/voice_app.c`, tiny, negligible CPU/RAM.
- Qualities: speech remains intelligible; simple, deterministic; no extra library.

### MP3 (potential)

- Bit rate: user selectable, e.g. 32–64 kbps. 64 kbps = 8000 B/s (similar to IMA-ADPCM), 32 kbps = 4000 B/s.
- Decoder: none in ESP-IDF by default; would need an MP3 decoder library (Helix, minimp3, or the ESP-ADF MP3 component). ~30–40 KB flash for the decoder, plus RAM and CPU.
- Note: MP3 decode is more CPU-intensive than IMA-ADPCM.

### Opus (potential)

- Bit rate: user selectable, e.g. 6–24 kbps. 12 kbps = 1500 B/s, 24 kbps = 3000 B/s.
- Decoder: ESP-IDF has no built-in Opus decoder; need to port libopus or a component. On ESP32-C3 (no PSRAM), Opus needs ~60–80 KB flash (decoder) and a share of RAM; CPU cost is moderate.
- Opus is far more efficient than IMA-ADPCM at low bit rates for speech.

### Raw PCM (baseline, not used)

- 16 kHz mono 16-bit = 32000 B/s. ~4× larger than IMA-ADPCM. Not used in this product.

## Measured results

The OPUS numbers below were measured by real encoding with ffmpeg 4.4 (libopus) over clips from the current set, at three rate points. The measured bytes/second differ from the nominal kbps because per-segment container/frame overhead adds a little; the effective rate is what matters for capacity.

| Sample | Duration | Opus 6 kbps | Opus 12 kbps | Opus 24 kbps |
| --- | --- | --- | --- | --- |
| cxk (short) | 0.61 s | 867 B/s | 1593 B/s | 2871 B/s |
| mama (mid) | 2.55 s | 1084 B/s | 1623 B/s | 4284 B/s |
| ren sheng (long) | 14.02 s | 845 B/s | 1532 B/s | 3216 B/s |

Effective rates: **6 kbps ≈ 850–1000 B/s, 12 kbps ≈ 1500–1600 B/s, 24 kbps ≈ 2900–4300 B/s.** At the nominal rate the capacity numbers are as below.

## Capacity at 3 MB

The percent of the full current set that fits in the 3 MB partition, by method and bit rate. Higher is better, but the firmware decoder cost is the counterweight.

The IMA-ADPCM rows are **measured** over the current set (total ~35.1 MB, ~7800 B/s). The OPUS rows use the measured ~1500 B/s for 12 kbps and ~1000 B/s for 6 kbps; the MP3 rows are the nominal rate (no MP3 measurement run here, noted as nominal only).

| Method / bit rate | B/s | Fits in 3 MB | Share of set |
| --- | --- | --- | --- |
| IMA-ADPCM 4-bit | 7800 | ~402 s | ~8.7% |
| MP3 64 kbps (nominal) | 8000 | ~402 s | ~8.7% |
| MP3 32 kbps (nominal) | 4000 | ~805 s | ~17.5% |
| Opus 24 kbps | 4300 | ~1048 s | ~22.8% |
| Opus 12 kbps | 1500 | ~2146 s | ~46.7% |
| Opus 6 kbps | 1000 | ~3072 s | ~66.8% |

### Measured select set

Even by the most efficient packing (longest duration per KB), the 3 MB partition holds only 8 of 38 directories under IMA-ADPCM, covering ~9.4% of total duration. The largest, most popular packs (each exceeding a third of the partition) are the first to be dropped:

- Jile voice pack (4385 KB, 570 s) — alone exceeds the whole partition
- Kenan voice pack (3172 KB, 413 s)
- Hajimi (3089 KB, 398 s)
- JoJo voice pack (2505 KB, 323 s)

This is the core reason a codec change matters: under IMA-ADPCM the capacity is fixed by physics, and no selection recovers the bulk of the content.

## Firmware decoder cost

This is the decisive counterweight to raw compression ratio.

| Method | Decoder needed | Flash (est.) | RAM (est.) | CPU |
| --- | --- | --- | --- | --- |
| IMA-ADPCM | already present | ~0 (already built) | negligible | very low |
| MP3 | add component | ~30–40 KB | small | moderate |
| Opus | add libopus | ~60–80 KB | moderate | moderate |

On ESP32-C3 with no PSRAM, the extra flash and RAM for an Opus/MP3 decoder must be budgeted against the application and the LVGL UI, and the partition could equally be enlarged to fit more IMA-ADPCM content. The right choice depends on whether raw capacity or firmware simplicity matters more.

## Recommendation summary

The measured numbers settle the trade-off. Under the current IMA-ADPCM codec, the 3 MB partition holds only ~8.7% of the current asset set, and the largest packs individually exceed the whole partition. That is a hard physical ceiling.

Opus changes the arithmetic materially:

- **Opus 12 kbps** fits ~46.7% of the set — a 5× improvement over IMA-ADPCM, at still-intelligible speech.
- **Opus 6 kbps** fits ~66.8%, but edges toward marginal intelligibility.
- The cost is a decoder: ~60–80 KB flash and some RAM on a chip with no PSRAM, plus moderate CPU. It must be budgeted against the LVGL UI and the app.

Two viable paths, in order of least risk:

1. **Keep IMA-ADPCM, enlarge the partition.** The 8 MB flash has ~1.94 MB unallocated; growing `voicefs` from 3 MB to ~5 MB raises the IMA-ADPCM ceiling to ~13.7%. No new decoder, minimal firmware change, but still only about a seventh of the set.
2. **Add a small Opus decoder and keep 3 MB.** At 12 kbps this recovers nearly half the set for the same partition footprint, but requires porting a decoder and re-validating decode CPU/RAM on the device.

If the product goal is "make as much of the current set playable as possible," path 2 (Opus) delivers the most content per flash; if the goal is "no new firmware risk," path 1 (enlarge the partition with the existing codec) is the safer increment. Either way, the current asset set cannot fully fit in 3 MB without a codec change.
