<p align="right">
  <a href="network-audio-streaming-and-memory.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Network Audio Streaming and Memory Budgeting on AI Passport

This note collects reusable lessons from adding HTTP audio streaming to AI
Passport. It is intentionally application-neutral: the focus is the board's
ESP32-C3, ES8311 audio path, display workload, and no-PSRAM memory limit.

## Start from the board boundary

AI Passport has 8 MB flash and no PSRAM. The ST7789P3 display, Wi-Fi/TLS,
decoder, HTTP client, JSON parser, and LVGL therefore compete for internal RAM.
The ES8311 and CW2017 also share I2C0, so application code must reuse the BSP
bus instead of creating another driver instance.

The authoritative pin and bus definitions are in
[`bsp_pins.h`](../../../components/bsp/include/bsp_pins.h):

| Signal | AI Passport connection |
| --- | --- |
| ES8311 control | I2C0, SDA GPIO10, SCL GPIO7, 7-bit address `0x18` |
| I2S clocks | MCLK GPIO6, BCLK GPIO5, WS GPIO3 |
| I2S data | DOUT GPIO2, DIN GPIO4 |
| Display | ST7789P3, 240 × 320 |

Use [`bsp_audio`](../../../components/bsp/src/bsp_audio.c) as the owner of the
codec and I2S channels. PCM reads and writes are blocking operations and belong
in worker tasks, never in LVGL or button callbacks.

## Keep the streaming pipeline bounded

A stable playback pipeline has explicit ownership at every stage:

1. A controller validates the request and starts one playback worker.
2. The worker opens the HTTP stream and accepts both fixed and unknown content
   lengths.
3. A bounded input buffer feeds the decoder incrementally; the complete file is
   never accumulated in RAM.
4. Decoded PCM is converted to the format opened by the BSP and written to I2S.
5. One cleanup path closes HTTP, decoder, audio, and UI state on success,
   cancellation, timeout, and decode failure.

For MP3 with a Helix-style decoder, one measured starting point was a 24 KiB
compressed-input buffer, a 2304-sample PCM buffer, and an 8 KiB playback-task
stack. These are measurements from one firmware, not board defaults. Record
minimum free heap, largest free block, and task stack high-water marks, then
shrink or grow the buffers from evidence.

Do not assume every decoded frame matches the target output format. Open the
codec using the source sample rate, convert stereo to mono when the application
needs mono, and calculate progress from decoded PCM samples. Byte-based progress
is misleading for variable-bit-rate streams and unavailable for chunked HTTP.

## Treat memory as one system budget

The common failure is not simply “the audio buffer is too small.” A firmware may
run out of one large contiguous block while total free heap still looks healthy.
Before each expensive phase, log both free heap and largest free block.

Budget these consumers together:

- TLS and HTTP receive buffers;
- decoder input, PCM output, and decoder state;
- I2S DMA descriptors and DMA buffers;
- LVGL draw buffers, image decoders, and screen objects;
- JSON documents and temporary response strings;
- worker-task stacks.

On the upstream BSP, LVGL uses a 20-row draw buffer, about 9.6 KiB at RGB565.
That is a useful baseline because enlarging the display buffer or I2S DMA ring
directly reduces headroom for networking and decoding. Change one buffer family
at a time and repeat playback while the UI is actively refreshing.

## Size JSON by the response, not the request

One real crash pattern came from parsing a response with a fixed 4096-byte JSON
document while the actual payload required about 6971 bytes. The request itself
was small, so request-size testing did not reveal the problem.

Safer rules are:

- reject an oversized HTTP response before parsing when its length is known;
- when the length is unknown, accumulate only up to an explicit cap;
- use a parser capacity derived from the accepted response limit;
- check deserialization errors and stop before reading missing fields;
- keep large metadata responses out of the playback worker when possible.

An optional relay service can normalize large or unstable upstream APIs into a
small device contract, but it should not hide firmware limits. Keep device-side
caps, timeouts, and error handling even when a relay is used.

## Concurrency and UI rules

Allow only one owner of the audio device. A new request should either be rejected
or cancel and join the existing worker before another worker opens I2S. Button
callbacks should post commands; they must not perform DNS, HTTP, decode, or PCM
writes.

Reuse the playback screen instead of recreating a full object tree on every
track. Update progress at a modest rate and modify only the changed labels or
bar. This reduces heap fragmentation and prevents display work from starving
the decoder.

## Verification checklist

- Play fixed-length and chunked streams, including a stream without
  `Content-Length`.
- Exercise mono/stereo, supported sample rates, malformed frames, slow network,
  server disconnect, cancellation, and repeated track changes.
- Refresh the display during playback and verify that audio does not underrun.
- Log free heap, largest free block, and stack high-water marks before connect,
  after TLS, after decoder creation, during steady playback, and after cleanup.
- Repeat start/stop cycles and confirm memory returns to a stable baseline.

The reusable lesson is to make every buffer and owner explicit. On AI Passport,
streaming is reliable when network, decoder, I2S, display, and JSON memory are
designed as one bounded pipeline rather than tuned independently.
