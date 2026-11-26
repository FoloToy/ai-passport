<p align="right">
  <a href="network-audio-streaming.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Network Audio Streaming and Memory Budgeting on AI Passport

Captured after the network-audio playback work on the AI Passport firmware (the DLNA/UPnP media-renderer work that streams MP3 over HTTP into the on-board ES8311). This entry records how to stream MP3, drive the on-board codec, and internal-RAM budget when HTTP, DMA, task stacks, and the LVGL UI all compete for the same no-PSRAM memory. It is not tied to one application: network radio, voice announcements, an online story player, a music player, or any other app that produces sound can use the same approach.

The hardware facts below follow the AI Passport board definitions, not a generic ESP32-C3 development board. The main references are [`bsp_pins.h`](../../../components/bsp/include/bsp_pins.h) and [`bsp_audio.c`](../../../components/bsp/src/bsp_audio.c).

## AI Passport hardware boundary

| Item | AI Passport actual configuration | Impact on development |
| --- | --- | --- |
| MCU | ESP32-C3, 8 MB flash, no PSRAM | Audio, Wi-Fi, TLS, LVGL, task stacks and DMA all compete for internal RAM |
| Display | ST7789P3, 240×320, RGB565, SPI2 40 MHz | A large double display buffer directly squeezes the audio DMA and decode buffers |
| Audio | On-board ES8311, microphone and speaker | Reuse the BSP; do not build another codec/I2S driver in the application |
| I2C0 | SDA GPIO10, SCL GPIO7 | ES8311 `0x18` and CW2017 `0x63` share the bus; do not create a second I2C0 bus |
| I2S0 | MCLK GPIO6, BCLK GPIO5, WS GPIO3, DOUT GPIO2, DIN GPIO4 | MCU is master, ES8311 is slave; playback uses DOUT, recording uses DIN |
| Power-amp enable | `-1` in the BSP | Current hardware treats the power amp as always-on; do not assume a controllable PA GPIO |
| Function keys | UP, DOWN, OK share a GPIO0 ADC resistor ladder | Callbacks come from the button task; do not decode, connect, or block on audio writes in a callback |
| Log interface | USB Serial/JTAG, GPIO18/19 | Do not switch back to the default UART0; GPIO21 is already used for the display backlight |

The 20-line LCD single buffer, 24 KB LVGL pool, audio task stack, and I2S DMA described in this document are the balance made around this set of hardware resources. They can be re-tuned when porting to other hardware. When continuing AI Passport development, measure first, then modify.

## 1. Decide the boundary first: the device only receives what it can use

If the audio address is fixed, needs no login, is TLS-compatible, and returns a small amount of data, the device can reach the audio source directly without adding a relay layer just for architectural completeness. Add a relay layer when third-party authorization, large JSON, temporary URLs, hotlink protection, or complex protocols appear.

The recommended chain with a relay layer:

```text
AI Passport device
  ├─ control commands, content lists, playback URLs: small JSON
  └─ MP3 byte stream
          ↓
optional relay service
  ├─ holds third-party authorization credentials
  ├─ calls upstream business interfaces
  ├─ trims fields and pagination
  └─ proxies temporary audio URLs
          ↓
third-party API and audio source
```

This split is not "add a layer" for its own sake; it moves the work the device is not good at away from it:

- Firmware does not hold third-party authorization credentials, and does not need to adapt to the often-changing large JSON from upstream.
- The relay service returns only the fields the device will use, so the response size stays stable and controllable.
- The audio URL can become a LAN relay address, reducing TLS, redirect, hotlink-protection and CDN compatibility problems.
- When the upstream interface changes, only the server-side adapter changes; the firmware protocol stays the same.

A common module boundary:

- `api_client`: request authorization, small JSON receive and parse.
- `app_controller`: page state machine, background network task, playback control.
- `audio_stream`: HTTP pull, MP3 decode, PCM output, and progress accounting.
- `bsp_audio`: I2S and ES8311 initialization, format switching, write, volume, and release.
- optional relay service: third-party authorization state, data trimming, paging, and audio proxy.

## 2. Firmware directory and how to wire dependencies

If the firmware is built on an existing project, put custom components outside the main project directory to reduce intrusiveness into upstream code:

```cmake
# firmware/device-app/CMakeLists.txt
set(EXTRA_COMPONENT_DIRS "../components")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(network_audio_device)
```

The audio component exposes only stable interfaces; the application layer should not call Helix or I2S directly:

```c
esp_err_t audio_stream_start(const char *url);
esp_err_t audio_stream_pause(void);
esp_err_t audio_stream_resume(void);
void audio_stream_request_stop(void);  // non-blocking request
void audio_stream_stop(void);          // waits for the task to exit
audio_stream_state_t audio_stream_state(void);
uint32_t audio_stream_elapsed_ms(void);
esp_err_t audio_stream_set_volume(int percent);
```

Example component dependency:

```yaml
dependencies:
  idf: ">=5.5.3,<5.6.0"
  chmorgan/esp-libhelix-mp3: "^1.0.1"
```

```cmake
idf_component_register(
    SRCS "audio_stream.c" "mp3_decoder_helix.c"
    INCLUDE_DIRS "include" "."
    REQUIRES bsp esp_http_client esp-tls
)
```

## 3. How to write the audio chain

### 3.1 Streaming read is mandatory; do not load a whole song into memory

A full MP3 can be several MB, and an ESP32-C3 cannot download then play. The correct approach is to prepare a finite input buffer and loop:

```text
HTTP read → find MP3 frame header → Helix decode → stereo to mono → I2S write out
```

A set of reference starting points verified on a no-PSRAM ESP32-C3:

```c
#define AUDIO_STREAM_INPUT_BYTES   (24 * 1024)
#define AUDIO_STREAM_PCM_SAMPLES   2304
#define AUDIO_STREAM_TASK_STACK    8192
#define AUDIO_STREAM_TASK_PRIORITY 6
```

These values are not "bigger is better." The input buffer, PCM buffer, decoder, task stack, HTTP buffer, LVGL memory pool, and DMA all occupy internal RAM at the same time. After changing one value, observe the overall remaining heap and the largest contiguous free block.

The core loop should keep four actions:

```c
for (;;) {
    wait_if_paused();
    compact_buffer(input, &bytes_left, &read_ptr);
    read_more_from_http(input + bytes_left, input_capacity - bytes_left);

    int sync = MP3FindSyncWord(read_ptr, bytes_left);
    if (sync < 0) {
        keep_last_byte_and_continue();
        continue;
    }

    int err = MP3Decode(decoder, &read_ptr, &bytes_left, pcm, 0);
    if (err != 0) {
        log_decode_error_with_buffered_bytes(err, bytes_left);
        continue;
    }

    MP3GetLastFrameInfo(decoder, &frame);
    set_codec_format_when_sample_rate_changes(frame.samprate);
    downmix_stereo_to_mono_if_needed(pcm, &frame);
    bsp_audio_write(pcm, pcm_bytes);
}
```

Implementation details worth noting:

- Detect and skip the ID3v2 tag first; otherwise a large leading tag can keep the decoder from finding a frame for a long time.
- HTTP `Content-Length` may be `-1`, meaning chunked transfer or unknown length; as long as the status is 2xx, allow streaming to continue.
- Do not resample all audio to a fixed sample rate unless there is a clear need. Reconfigure the codec by the MP3 source sample rate to reduce computation and improve quality.
- When the ES8311 output is mono, merge two-channel PCM with `(left + right) / 2`; use a wide enough intermediate type to avoid overflow on the sum.
- Accumulate playback progress by the PCM samples actually written, not by an estimate of downloaded bytes.

### 3.2 Initialize and release audio hardware on demand

Do not hold I2S and the codec long-term from the start of the system. Recommended order:

```text
open HTTP and confirm the response is normal
→ allocate input/PCM/decoder
→ initialize I2S and ES8311
→ open the codec at the source sample rate after the first frame decodes
→ play
→ close HTTP, release decoder and buffers
→ delete codec/I2S handles
```

This leaves the memory for network and UI during provisioning, TLS handshake, and complex UI stages, and also avoids half-initialized handles after an audio init failure.

All failure exits should merge into one cleanup section that frees each item after checking whether it was created. Do not write a separate release path in each branch.

### 3.3 I2S DMA should cover brief stalls

Reference values verified on real hardware:

```c
i2s_chan_config_t chan = {
    .role = I2S_ROLE_MASTER,
    .dma_desc_num = 8,
    .dma_frame_num = 512,
    .auto_clear_after_cb = true,
};
```

A deeper DMA absorbs brief stalls like an LVGL refresh or an I2C volume change. Too small shows up as dropouts, pops, or periodic stutter; too large takes DMA memory away from the display and can make the display or I2S init report `ESP_ERR_NO_MEM`.

AI Passport has no PSRAM and the display must also give internal RAM to audio. The current 240×320 RGB565 display uses a 20-line single buffer, about 9.6 KB:

```c
.buffer_size = BSP_LCD_W * 20,
.double_buffer = false,
```

Do not change the display to a large double buffer, enlarge the audio DMA, enlarge the HTTP buffer, and enlarge the task stack all at once, then only look at total free heap. Many peripherals need "contiguous memory with a specific capability"; total free can be high and an allocation can still fail.

### 3.4 Common details of the AI Passport on-board ES8311

- The AI Passport ES8311 uses 7-bit address `0x18` on I2C0, but the `esp_codec_dev` control interface requires the shifted 8-bit form, `0x18 << 1`. Do not copy this shift into normal ESP-IDF 7-bit address APIs.
- When the MCU is the I2S master, configure ES8311 as slave and enable MCLK.
- Reuse the I2C0 bus already created by `bsp_i2c`. Creating another bus for a temporary scan or codec init affects both the ES8311 and the fuel gauge.
- `esp_codec_dev_open()` reconfigures I2S by sample rate. When re-setting the same format, reuse it; only close/open when the format changes.
- If the driver prints "channel has not been enabled yet" when switching between READY/RUNNING, check the `enable → codec open → close → enable → reopen` order first, rather than simply suppressing the log.
- A volume change only updates the codec and the volume popup; do not rebuild the whole playback page.

## 4. The most common crash: not one buffer, but a group of memory peaks

### 4.1 A real case: JSON over 4096 bytes

The firmware HTTP JSON receive limit is:

```c
#define API_CLIENT_RESPONSE_MAX_BYTES 4096
```

In one real case the server returned 67 list items, about 6971 bytes, while the device had prepared a 4096-byte receive area. The event callback found:

```c
if (response->len + event->data_len >= response->cap) {
    response->overflow = true;
    return ESP_FAIL;
}
```

It finally returned `ESP_ERR_INVALID_SIZE`. In this case the first reaction should not be to change 4096 to 8192 or more, because the parsed array keeps occupying memory, and page objects create new peaks.

The right fix is to shrink the protocol first:

- Set an explicit cap on the total list count, for example 20 items.
- Return details page by page, for example keep at most 30 items per page.
- Limit ordinary text fields to 63 bytes and longer description fields to 95 bytes; adjust the budget by device memory.
- Drop fields the device does not use: image details, long descriptions, permissions, statistics.
- Truncate by UTF-8 byte count on the server before returning, not only by JavaScript character count.
- Firmware still keeps a hard cap and a clear error, so a server regression cannot write out of bounds.

Experience principle: the firmware buffer is the last line of defense; the server response budget is the first line of defense.

### 4.2 A real case: restart on page switch

Another "buffer not enough" is not an HTTP overflow but LVGL holding both the old and new object sets when switching pages. A no-PSRAM device hit a memory peak creating long list text objects, then an object creation failed and the device restarted.

The fix:

```c
lv_obj_t *screen = current_screen;
if (screen == NULL) {
    screen = create_screen();
} else {
    lv_obj_clean(screen);
    setup_screen(screen, next_screen);
}
```

Also require every UI creation function to check its return value:

```c
lv_obj_t *label = lv_label_create(parent);
if (label == NULL) {
    ESP_LOGE(TAG, "create label failed");
    return NULL;
}
```

The playback page should not rebuild the whole page every second for progress; only update the progress-bar width and the two time labels.

### 4.3 First decide which kind of "not enough"

| Symptom | More likely cause | First check |
| --- | --- | --- |
| HTTP returns `ESP_ERR_INVALID_SIZE` | JSON receive limit exceeded | actual response bytes, pagination, field trimming |
| `calloc` or `xTaskCreate` fails | normal heap or contiguous memory insufficient | current free heap, largest contiguous block, parallel tasks |
| I2S/LCD init `ESP_ERR_NO_MEM` | DMA-capable memory insufficient | display double buffer, I2S DMA depth |
| Restart right after switching to a list page | LVGL object peak or null pointer | whether the screen is reused, object creation return values |
| Stutter a few seconds into playback | DMA too shallow or high-priority task blocking | audio task priority, full-page UI refresh, I2C operations |
| MP3 keeps failing decode and the input buffer is full | not MP3, ID3/frame sync, or a proxy error page | HTTP status, Content-Type, leading bytes, decode error codes |

Log at key stages:

```c
ESP_LOGI(TAG, "before audio: free=%u largest=%u",
         heap_caps_get_free_size(MALLOC_CAP_8BIT),
         heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
```

Also record the cumulative response length, the page name, the task creation result, the MP3 frame sample rate, and the DMA init error. Logs should distinguish "network failure", "protocol too large", "out of memory", and "format not supported" rather than showing all as "playback failed".

## 5. When a relay layer is needed, how to write the audio proxy

If the audio source has temporary authorization, hotlink protection, complex redirects, or device TLS compatibility problems, the device does not need the real source address. The relay service can generate a short-lived ticket for each playback:

```text
GET /api/v1/audio/<ticket>.mp3
```

The server saves `ticket → upstream URL → expiry`, and streams the response directly when the request arrives:

```ts
const upstream = await fetch(ticket.url, {
  headers: {
    "user-agent": "Mozilla/5.0",
    referer: "https://music.163.com/",
  },
});

reply.header("cache-control", "no-store");
if (upstream.headers.get("content-length")) {
  reply.header("content-length", upstream.headers.get("content-length"));
}
return reply.type(upstream.headers.get("content-type") ?? "audio/mpeg")
  .send(Readable.fromWeb(upstream.body));
```

Key points:

- The server also streams; do not read a whole song into Node.js memory.
- The ticket should be short-lived, cleaned up on expiry, and never expose the real address to the device long-term.
- If upstream returns 404, 403, or no body, return a clear error to the device; do not disguise a JSON error page as MP3.
- If the firmware only supports MP3, the relay service should reject other formats before returning the playback URL.

## 6. Concurrency and state machine experience

- LVGL is not thread-safe. Network and audio run in background tasks; the UI only updates on the LVGL task or while holding the lock.
- Button callbacks must return quickly, only change state or notify a task, and must not do an HTTP request in the callback.
- Ignore duplicate requests while the background task is busy, so repeated presses do not produce multiple authorization, page, or playback requests.
- `request_stop()` sends a quick stop signal; call the blocking `stop()` when you need to ensure resources are released.
- When the audio layer has naturally ended, the control layer should show ended or return to the content page, and not continue to mark the UI as paused successfully.
- After a long press, a release or single-click event usually follows; set a short ignore flag, otherwise one long press triggers two actions.

## 7. Recommended validation order

1. Verify authorization, list, detail, and playback URL against a local mock service first, confirming the JSON contract.
2. Use a very short local MP3 to verify HTTP, Helix, sample-rate switching, and I2S.
3. Then connect the real business interface and record the actual byte count of each response.
4. Continuously switch list, content, and audio, and watch whether the remaining heap and largest contiguous block keep dropping.
5. During playback, repeatedly open the volume popup, page, and return to the list; check for stutter or restart.
6. Test audio responses with and without `Content-Length` (chunked transfer) separately.
7. Play multiple MP3s at different sample rates and single/double channels consecutively.

The completion bar is not "it makes sound once" but that network failure, oversized response, unsupported format, and out-of-memory all leave a clear log and safely return to an operable page.

## 8. Directly reusable conclusions

- AI Passport has no PSRAM; new features must budget display, audio, Wi-Fi, TLS, task stacks, and DMA as one whole.
- Audio pins, I2C addresses, and bus ownership follow the AI Passport BSP, not a generic ESP32-C3 example.
- Limit the server response first, then decide the firmware buffer; do not hide a protocol runaway with more memory.
- For a no-PSRAM device, design for the "simultaneously active peaks", not only each module's static size.
- Audio, network, and UI must decouple; the UI does not decode, and the audio task does not touch LVGL directly.
- The player does only partial refresh; page switches reuse the screen object.
- HTTP audio must be streamed and accept unknown content length.
- I2S DMA and display buffers must be tuned together; either one hogging internal RAM makes the other fail.
- Check the return value of every allocation, object creation, and task creation; a failure must be clearly exposed and fully cleaned up.

## Route

This is general, upstream-benefiting hardware and design experience, so it is proposed back to the upstream `FoloToy/ai-passport` as a documentation PR.
