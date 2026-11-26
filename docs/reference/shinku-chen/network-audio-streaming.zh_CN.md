<p align="right">
  <strong>简体中文</strong> · <a href="network-audio-streaming.md">English</a>
</p>

# AI Passport 上的网络音频流式播放与内存预算

记录于 AI Passport 固件的网络音频播放工作之后（把 MP3 经 HTTP 流式送进板载 ES8311 的 DLNA/UPnP 媒体渲染器工作）。本条目记录如何流式解码 MP3、驱动板载 ES8311，以及在 HTTP、DMA、任务栈和 LVGL 界面同时争抢无 PSRAM 内部内存时如何做内存预算。它不限定具体应用，基于 AI Passport 开发网络收音机、语音播报器、在线故事机、音乐播放器或其他带声音的应用，都可以采用相同思路。

文中的硬件事实以 AI Passport 的板级定义为准，不按普通 ESP32-C3 开发板猜测。主要依据是 [`bsp_pins.h`](../../../components/bsp/include/bsp_pins.h) 和 [`bsp_audio.c`](../../../components/bsp/src/bsp_audio.c)。

## AI Passport 硬件边界

| 项目 | AI Passport 实际配置 | 对开发的影响 |
| --- | --- | --- |
| MCU | ESP32-C3，8 MB Flash，无 PSRAM | 音频、Wi-Fi、TLS、LVGL、任务栈和 DMA 全部争抢内部 RAM |
| 屏幕 | ST7789P3，240×320，RGB565，SPI2 40 MHz | 大显示双缓冲会直接挤压音频 DMA 和解码缓冲 |
| 音频 | 板载 ES8311、麦克风和扬声器 | 应复用 BSP，不要在应用里另建 codec/I2S 驱动 |
| I2C0 | SDA GPIO10、SCL GPIO7 | ES8311 `0x18` 与 CW2017 `0x63` 共用总线，不能创建第二个 I2C0 bus |
| I2S0 | MCLK GPIO6、BCLK GPIO5、WS GPIO3、DOUT GPIO2、DIN GPIO4 | MCU 为 master，ES8311 为 slave；播放使用 DOUT，录音使用 DIN |
| 功放使能 | BSP 中为 `-1` | 当前硬件按功放常开处理，不能假设有可控制的 PA GPIO |
| 功能按键 | UP、DOWN、OK 共用 GPIO0 ADC 电阻梯 | 回调来自按键任务，不能在回调里解码、联网或阻塞写音频 |
| 日志接口 | USB Serial/JTAG，GPIO18/19 | 不要直接改回默认 UART0；GPIO21 已用于屏幕背光 |

这份文档里的 20 行 LCD 单缓冲、24 KB LVGL 池、音频任务栈和 I2S DMA，都是围绕这组硬件资源做的平衡。移植到其他硬件时可以重调；继续开发 AI Passport 时，应先测量再修改。

## 1. 先确定边界：设备只接收自己用得上的数据

如果音频地址固定、无需登录、TLS 兼容且返回数据很小，设备可以直接访问音频源，不必为了架构完整而增加中转层。出现第三方授权、大型 JSON、临时 URL、防盗链或复杂协议时，再增加中转层。

带中转层的推荐链路：

```text
AI Passport 设备
  ├─ 控制命令、内容列表、播放地址：小型 JSON
  └─ MP3 字节流
          ↓
可选的中转服务
  ├─ 保存第三方授权凭据
  ├─ 调用上游业务接口
  ├─ 裁剪字段和分页
  └─ 代理临时音频地址
          ↓
第三方 API 和音频源
```

这样拆分不是为了"多加一层"，而是把设备不擅长的工作移走：

- 固件不保存第三方授权凭据，也不需要适配上游经常变化的大 JSON。
- 中转服务只返回设备会用到的字段，响应大小可以稳定控制。
- 音频 URL 可以变成局域网中转地址，减少 TLS、重定向、防盗链和 CDN 兼容问题。
- 上游接口变化时，只改服务端适配层，固件协议保持不变。

一种通用的模块边界如下：

- `api_client`：请求鉴权、小型 JSON 接收和解析。
- `app_controller`：页面状态机、后台网络任务、播放控制。
- `audio_stream`：HTTP 拉流、MP3 解码、PCM 输出和进度统计。
- `bsp_audio`：I2S 和 ES8311 的初始化、格式切换、写入、音量和释放。
- 可选中转服务：第三方授权状态、数据裁剪、分页和音频代理。

## 2. 固件目录和依赖怎么接

如果固件基于已有工程二次开发，可把自定义组件放在主工程目录外侧，减少对上游代码的侵入：

```cmake
# firmware/device-app/CMakeLists.txt
set(EXTRA_COMPONENT_DIRS "../components")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(network_audio_device)
```

音频组件只暴露稳定接口，应用层不要直接调用 Helix 或 I2S：

```c
esp_err_t audio_stream_start(const char *url);
esp_err_t audio_stream_pause(void);
esp_err_t audio_stream_resume(void);
void audio_stream_request_stop(void);  // 非阻塞请求
void audio_stream_stop(void);          // 等待任务退出
audio_stream_state_t audio_stream_state(void);
uint32_t audio_stream_elapsed_ms(void);
esp_err_t audio_stream_set_volume(int percent);
```

组件依赖示例：

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

## 3. 音频链路怎么写

### 3.1 必须流式读取，不能整首歌放进内存

整首 MP3 可能有几 MB，ESP32-C3 无法先下载再播放。正确做法是准备一个有限输入缓冲区，循环执行：

```text
HTTP 读取 → 查找 MP3 帧头 → Helix 解码 → 立体声转单声道 → I2S 写出
```

一组在无 PSRAM ESP32-C3 上验证过的参考起点：

```c
#define AUDIO_STREAM_INPUT_BYTES   (24 * 1024)
#define AUDIO_STREAM_PCM_SAMPLES   2304
#define AUDIO_STREAM_TASK_STACK    8192
#define AUDIO_STREAM_TASK_PRIORITY 6
```

这些值不是越大越好。输入缓冲、PCM 缓冲、解码器、任务栈、HTTP 缓冲、LVGL 内存池和 DMA 都会同时占用内部 RAM。改一个值后必须观察整体剩余堆和最大连续空闲块。

核心循环建议保留四个动作：

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

实现时还要注意：

- 先识别并跳过 ID3v2 标签，否则开头的大标签可能让解码器长时间找不到帧。
- HTTP `Content-Length` 可能是 `-1`，表示分块传输或未知长度；只要状态码是 2xx，就应允许继续流式读取。
- 不要在没有明确需要时把所有音频重采样成固定采样率。可按 MP3 源采样率重配 codec，减少计算量并改善音质。
- ES8311 输出为单声道时，双声道 PCM 用 `(left + right) / 2` 合并；要用足够宽的中间类型，避免相加溢出。
- 播放进度按实际写入的 PCM 样本累计，而不是用下载字节数估算。

### 3.2 音频硬件按需初始化和释放

不要在系统启动时就长期占着 I2S 和 codec。推荐顺序：

```text
打开 HTTP 并确认响应正常
→ 分配输入/PCM/解码器
→ 初始化 I2S 和 ES8311
→ 解出第一帧后按源采样率打开 codec
→ 播放
→ 关闭 HTTP、释放解码器和缓冲
→ 删除 codec/I2S 句柄
```

这能把配网、TLS 握手和复杂界面阶段的内存留给网络与 UI，也能避免音频初始化失败后留下半初始化句柄。

所有失败出口都应汇合到一个清理段，逐项判断是否已创建再释放。不要在多个分支里各写一套释放逻辑。

### 3.3 I2S DMA 要覆盖短暂停顿

下面是一组经过真机验证的参考值：

```c
i2s_chan_config_t chan = {
    .role = I2S_ROLE_MASTER,
    .dma_desc_num = 8,
    .dma_frame_num = 512,
    .auto_clear_after_cb = true,
};
```

较深的 DMA 可以吸收 LVGL 刷新、I2C 调音量等短暂停顿。过小会表现为断音、爆音或周期性卡顿；过大则会抢占内部 DMA 内存，导致显示或 I2S 初始化报 `ESP_ERR_NO_MEM`。

AI Passport 无 PSRAM，显示也要为音频让出内部 RAM。当前 240×320、RGB565 屏幕使用 20 行单缓冲，约 9.6 KB：

```c
.buffer_size = BSP_LCD_W * 20,
.double_buffer = false,
```

不要同时把显示改成大双缓冲、增大音频 DMA、增大 HTTP 缓冲和增大任务栈，然后只看总空闲堆。很多外设需要的是"具有特定能力的连续内存"，总空闲不少也可能分配失败。

### 3.4 AI Passport 板载 ES8311 常见细节

- AI Passport 的 ES8311 在 I2C0 上使用 7 位地址 `0x18`，但 `esp_codec_dev` 控制接口要求传入左移后的 8 位形式，即 `0x18 << 1`。不要把这个左移复制到普通 ESP-IDF 7 位地址 API。
- MCU 是 I2S master 时，ES8311 配为 slave，并启用 MCLK。
- 必须复用 `bsp_i2c` 已创建的 I2C0 bus；临时扫描或初始化 codec 时另建总线，会同时影响 ES8311 和电量计。
- `esp_codec_dev_open()` 会按采样率重配 I2S。重复设置相同格式时直接复用，格式变化时才 close/open。
- 如果驱动在 READY/RUNNING 状态切换时打印"channel has not been enabled yet"，先核对 `enable → codec open → close → enable → reopen` 的顺序，不要简单屏蔽日志。
- 音量变化只更新 codec 和音量弹窗，不要重建整个播放页。

## 4. 最常见的死机：不是一个缓冲区，而是一组内存峰值

### 4.1 真实案例：JSON 超过 4096 字节

固件 HTTP JSON 接收上限是：

```c
#define API_CLIENT_RESPONSE_MAX_BYTES 4096
```

一个真实案例中，服务端一次返回 67 条列表数据，响应约 6971 字节，而设备只准备了 4096 字节接收区。事件回调发现：

```c
if (response->len + event->data_len >= response->cap) {
    response->overflow = true;
    return ESP_FAIL;
}
```

最后返回 `ESP_ERR_INVALID_SIZE`。这种情况下，第一反应不应是把 4096 改成 8192 或更大，因为解析后的数组也会继续占内存，页面对象又会制造新的峰值。

正确修复是先缩协议：

- 列表总数设置明确上限，例如 20 条。
- 明细按页返回，例如每页最多保存 30 条。
- 普通文字字段限制为 63 字节，较长的说明字段限制为 95 字节；具体预算按设备内存调整。
- 删除图片详情、长描述、权限、统计等设备不用的字段。
- 服务端返回前按 UTF-8 字节数截断，不能只按 JavaScript 字符数截断。
- 固件仍保留硬上限和明确错误，防止服务端回归后写越界。

经验原则：固件缓冲区是最后一道防线，服务端响应预算才是第一道防线。

### 4.2 真实案例：页面切换时重启

另一种"缓冲区不够"不是 HTTP 溢出，而是 LVGL 切页时同时存在新旧两套对象。一个无 PSRAM 设备在创建长列表文字对象时达到内存峰值，随后对象创建失败并重启。

修复方式：

```c
lv_obj_t *screen = current_screen;
if (screen == NULL) {
    screen = create_screen();
} else {
    lv_obj_clean(screen);
    setup_screen(screen, next_screen);
}
```

同时要求所有 UI 创建函数检查返回值：

```c
lv_obj_t *label = lv_label_create(parent);
if (label == NULL) {
    ESP_LOGE(TAG, "create label failed");
    return NULL;
}
```

播放页进度也不能每秒重建整页，只更新进度条宽度和两个时间标签。

### 4.3 先判断是哪种"不够"

| 表现 | 更可能的原因 | 首先检查 |
| --- | --- | --- |
| HTTP 返回 `ESP_ERR_INVALID_SIZE` | JSON 接收上限被超过 | 实际响应字节数、分页和字段裁剪 |
| `calloc`、`xTaskCreate` 返回失败 | 普通堆或连续内存不足 | 当前空闲堆、最大连续块、并行任务 |
| I2S/LCD 初始化 `ESP_ERR_NO_MEM` | DMA 能力内存不足 | 显示双缓冲、I2S DMA 深度 |
| 切换到列表页立刻重启 | LVGL 对象峰值或空指针 | 是否复用屏幕、对象创建返回值 |
| 播放几秒后卡顿 | DMA 太浅或高优先级任务阻塞 | 音频任务优先级、UI 全页刷新、I2C 操作 |
| MP3 一直解码失败且输入缓冲已满 | 非 MP3、ID3/帧同步或代理错误页 | HTTP 状态、Content-Type、开头字节和解码错误码 |

建议在关键阶段记录：

```c
ESP_LOGI(TAG, "before audio: free=%u largest=%u",
         heap_caps_get_free_size(MALLOC_CAP_8BIT),
         heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
```

同时记录响应累计长度、页面名、任务创建结果、MP3 帧采样率和 DMA 初始化错误。日志要能区分"网络失败""协议过大""内存不足"和"格式不支持"，不要都显示成"播放失败"。

## 5. 需要中转层时，音频代理怎么写

如果音频源存在临时鉴权、防盗链、复杂重定向或设备 TLS 兼容问题，设备不必拿到真实源地址。中转服务可为每次播放生成一个短期 ticket：

```text
GET /api/v1/audio/<ticket>.mp3
```

服务端保存 `ticket → 上游 URL → 过期时间`，请求到来时直接流式转发：

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

关键点：

- 服务端也要流式转发，不能把整首歌读进 Node.js 内存。
- ticket 要短期有效，过期及时清理，不能把真实地址长期暴露给设备。
- 上游返回 404、403 或没有 body 时，向设备返回明确错误，不能把 JSON 错误页伪装成 MP3。
- 如果固件只支持 MP3，中转服务拿到其他格式时应在返回播放 URL 前拒绝。

## 6. 并发和状态机经验

- LVGL 不是线程安全的。网络和音频放后台任务，UI 只在 LVGL 任务或持锁状态下更新。
- 按键回调必须快速返回，只改变状态或通知工作任务，不能在回调里执行 HTTP 请求。
- 后台任务忙时忽略重复请求，防止连按产生多个鉴权、翻页或播放请求。
- `request_stop()` 用于快速发停止信号；需要确保资源已经释放时再调用阻塞式 `stop()`。
- 音频底层已经自然结束时，控制层应显示已结束或返回内容页，不能继续把 UI 标成暂停成功。
- 长按后通常还会收到一次释放或单击事件，需要设置短时忽略标志，否则一次长按会触发两个动作。

## 7. 推荐的验证顺序

1. 先用本地模拟服务验证鉴权、列表、详情和播放 URL，确认 JSON 契约。
2. 用一个很短的本地 MP3 验证 HTTP、Helix、采样率切换和 I2S。
3. 再接真实业务接口，记录每种响应的实际字节数。
4. 连续切换列表、内容和音频，观察剩余堆和最大连续块是否持续下降。
5. 播放时反复打开音量弹窗、翻页和返回列表，检查是否卡顿或重启。
6. 分别测试有 `Content-Length` 和分块传输的音频响应。
7. 至少连续播放多首不同采样率、单双声道的 MP3。

完成标准不是"能响一次"，而是网络失败、响应过大、格式不支持和内存不足时都能留下明确日志，并安全回到可操作页面。

## 8. 可以直接复用的结论

- AI Passport 没有 PSRAM，新增功能时必须把显示、音频、Wi-Fi、TLS、任务栈和 DMA 作为一个整体计算。
- 音频引脚、I2C 地址和总线所有权以 AI Passport BSP 为准，不照搬通用 ESP32-C3 示例。
- 先限制服务端响应，再决定固件缓冲区；不要用加内存掩盖协议失控。
- 无 PSRAM 设备要按"同时活跃的峰值"设计，不要只看单个模块的静态大小。
- 音频、网络和 UI 必须解耦；界面不承担解码，音频任务不直接操作 LVGL。
- 播放器只做局部刷新，页面切换复用屏幕对象。
- HTTP 音频必须流式处理，并接受未知内容长度。
- I2S DMA 和显示缓冲需要一起调，任何一边独占内部 RAM 都会让另一边失败。
- 每个分配、对象创建和任务创建都检查返回值，失败要明确暴露并完整清理。

## 分流

这是通用、上游也受益的硬件与设计经验，因此作为文档 PR 提交回上游 `FoloToy/ai-passport`。
