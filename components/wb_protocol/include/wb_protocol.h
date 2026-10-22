// components/wb_protocol/include/wb_protocol.h
// WorkBuddy 设备 ↔ 主机协议 —— 传输层无关（当前用 USB-CDC 串口 / 原 BLE NUS 同形）
//
// 设备是 WorkBuddy 的「状态屏 + 审批遥控器」：屏幕显示状态/消息，三键批准/拒绝/翻看，
// 主机（WorkBuddy 大脑）通过串口下发状态、回复、审批请求与通知；设备回传按键与审批结果。
// 设备不跑 STT/LLM/TTS —— 大脑永远是 PC 侧的 WorkBuddy。
//
// 协议消息格式: 换行分隔的 UTF-8 JSON（串口直接用，PC 侧只解析以 '{' 开头的行）
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// BLE UUID (与大脑端保持一致)
// ============================================================
#define WB_SVC_UUID         "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define WB_CHAR_TX_UUID     "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define WB_CHAR_RX_UUID     "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

// 设备名称
#define WB_DEVICE_NAME       "FoloPassport"

// 固件标识与协议版本：PC 端在 pong 里读到它，用来确认刷的是哪一版
#define WB_FW_VERSION        "wb-2.0"
#define WB_PROTO_VERSION     2

// BLE 参数
#define WB_MTU              512
#define WB_TX_MAX_CHUNK     (WB_MTU - 16)   // 单包最大音频数据

// 单行 JSON 上限。TTS 播报分片带 base64（约 700 字节），故比 MTU 宽得多。
#define WB_RX_LINE_MAX      2048

// ============================================================
// 协议消息类型
// ============================================================

typedef enum {
    // --- 语音输入 ---
    WB_MSG_VOICE_BEGIN,       // PC → Device: 开始录音
    WB_MSG_VOICE_CHUNK,       // PC → Device: 音频分片（设备→PC 上行，hex PCM）
    WB_MSG_VOICE_END,         // PC → Device: 录音结束，开始处理
    WB_MSG_VOICE_TEXT,        // Device → PC: ASR 结果文字

    // --- 选项 / 审批选择 ---
    WB_MSG_SHOW_OPTIONS,      // PC → Device: 显示选项列表
    WB_MSG_OPTION_SELECTED,   // Device → PC: 选中的选项

    // --- 审批请求（状态屏 + 遥控器核心）---
    WB_MSG_ASK,              // PC → Device: 发起审批（"允许运行？"），等待 OK/DOWN
    WB_MSG_ANSWER,          // Device → PC: 审批结果 confirm / cancel

    // --- 通知推送 ---
    WB_MSG_NOTIFICATION,       // PC → Device: 通知
    WB_MSG_TASK_COMPLETE,     // PC → Device: 任务完成
    WB_MSG_TASK_ERROR,        // PC → Device: 任务失败

    // --- 状态 ---
    WB_MSG_STATUS,            // PC → Device: 状态推送（state + text）
    WB_MSG_PING,              // PC → Device: 链路探测
    WB_MSG_PONG,              // Device → PC: 链路响应
    WB_MSG_HEARTBEAT,         // PC → Device: 存活心跳（仅刷新最后可见时间，不改变应用状态）

    // --- 麦克风上行（设备 → PC，把卡片当 WorkBuddy 的麦克风）---
    WB_MSG_AUDIO,            // Device → PC: 麦克风音频分片（payload.b=base64 PCM, payload.s=seq）
    WB_MSG_AUDIO_END,        // Device → PC: 录音结束（缓冲排空后即可停止虚拟麦播放）

    // --- 远程调试 ---
    WB_MSG_DEBUG_TALK,       // PC → Device: 远程开始/停止采集（payload.on=true/false）
    WB_MSG_MIC_GAIN,         // PC → Device: 运行时改麦克风输入增益（payload.db，单位 dB）

    // --- 键盘事件 ---
    WB_MSG_KEY_EVENT,       // Device → PC: 按键事件（UP/DOWN/OK press/release）
    WB_MSG_KEY_ACKED,      // PC → Device: 按键已处理确认
    WB_MSG_CLEAR_SCREEN,   // PC → Device: 清屏

    // --- 助手回复（显示到对话记录）---
    WB_MSG_REPLY,          // PC → Device: 助手文字回复

    // --- 扬声器语音（PC 把 TTS 音频推给设备播报）---
    WB_MSG_TTS_BEGIN,      // PC → Device: 开始一次播报（payload.hz 采样率 / payload.a=1 表示 ADPCM）
    WB_MSG_TTS_CHUNK,      // PC → Device: 播报音频分片（payload.s=seq / payload.b=base64）
    WB_MSG_TTS_END,        // PC → Device: 播报音频结束
    WB_MSG_TTS_STOP,       // PC → Device: 立即停止播报并丢弃缓冲

    WB_MSG_HEADER,         // PC → Device: 标题栏文本（多会话时显示会话标签）

    WB_MSG_UNKNOWN,
} wb_msg_type_t;

// ============================================================
// 回调接口
// ============================================================

typedef struct wb_protocol_t wb_protocol_t;

// 收到文字注入回调（ASR 结果）
typedef void (*wb_on_voice_text_t)(wb_protocol_t *p, const char *text);
// 收到选项列表回调
typedef void (*wb_on_show_options_t)(wb_protocol_t *p, const char *options_json);
// 收到选项选中回调
typedef void (*wb_on_option_selected_t)(wb_protocol_t *p, const char *option_id);
// 收到审批请求回调（id + 文本）
typedef void (*wb_on_ask_t)(wb_protocol_t *p, const char *id, const char *text);
// 收到通知回调
typedef void (*wb_on_notification_t)(wb_protocol_t *p, const char *title, const char *body, const char *priority);
// 连接状态变化回调
typedef void (*wb_on_conn_state_t)(wb_protocol_t *p, bool connected, uint16_t conn_handle);
// 收到按键 ACK 回调（PC 已处理按键）
typedef void (*wb_on_key_acked_t)(wb_protocol_t *p, const char *button, const char *event, bool success);
// 收到清屏回调
typedef void (*wb_on_clear_screen_t)(wb_protocol_t *p);
// 收到助手文字回复回调（PC → Device）
typedef void (*wb_on_reply_t)(wb_protocol_t *p, const char *text);
// 收到存活心跳回调（仅用于链路活性检测；成功解析到任何消息都会触发）
typedef void (*wb_on_heartbeat_t)(wb_protocol_t *p);
// 收到 TTS 语音分片回调（已解码为 raw PCM；调用方负责缓存/播放）
typedef void (*wb_on_audio_out_t)(wb_protocol_t *p, int seq, const uint8_t *data, size_t len);
// 收到 TTS 语音结束回调（缓冲排空后即可停止播放）
typedef void (*wb_on_audio_end_t)(wb_protocol_t *p);
// 收到远程调试：开始/停止采集（on=true 开始说话，on=false 停止）
typedef void (*wb_on_debug_talk_t)(wb_protocol_t *p, bool on);
// 收到麦克风增益设置（单位 dB）
typedef void (*wb_on_mic_gain_t)(wb_protocol_t *p, float db);
// 收到播报开始（hz=采样率，adpcm=true 表示分片是 IMA ADPCM 压缩包）
typedef void (*wb_on_tts_begin_t)(wb_protocol_t *p, int hz, bool adpcm);
// 收到播报音频分片（已 base64 解码为原始字节）
typedef void (*wb_on_tts_chunk_t)(wb_protocol_t *p, int seq, const uint8_t *data, size_t len);
// 收到播报结束 / 要求立即停止
typedef void (*wb_on_tts_end_t)(wb_protocol_t *p);
// 收到标题栏文本（多会话时显示会话标签与数量）
typedef void (*wb_on_header_t)(wb_protocol_t *p, const char *text);

struct wb_protocol_t {
    // 连接状态
    bool              connected;
    uint16_t          conn_handle;

    // 回调
    wb_on_voice_text_t      on_voice_text;
    wb_on_show_options_t    on_show_options;
    wb_on_option_selected_t  on_option_selected;
    wb_on_ask_t             on_ask;
    wb_on_notification_t    on_notification;
    wb_on_conn_state_t      on_conn_state;
    wb_on_key_acked_t      on_key_acked;
    wb_on_clear_screen_t    on_clear_screen;
    wb_on_reply_t          on_reply;
    wb_on_heartbeat_t      on_heartbeat;
    wb_on_audio_out_t      on_audio_out;
    wb_on_audio_end_t      on_audio_end;
    wb_on_debug_talk_t     on_debug_talk;
    wb_on_mic_gain_t       on_mic_gain;
    wb_on_tts_begin_t      on_tts_begin;
    wb_on_tts_chunk_t      on_tts_chunk;
    wb_on_tts_end_t        on_tts_end;
    wb_on_header_t         on_header;

    // 内部
    void               *user_data;
    char                rx_buf[WB_RX_LINE_MAX];
    int                rx_len;
};

// ============================================================
// 初始化与生命周期
// ============================================================

// 创建协议实例（内部分配）
wb_protocol_t *wb_protocol_new(void);

// 销毁协议实例
void wb_protocol_free(wb_protocol_t *p);

// 设置用户数据
void wb_protocol_set_user_data(wb_protocol_t *p, void *data);
void *wb_protocol_get_user_data(wb_protocol_t *p);

// ============================================================
// 消息发送 (Device → PC)
// ============================================================

// 发送 ASR 结果文字
esp_err_t wb_send_voice_text(wb_protocol_t *p, const char *text);

// 发送选项选中
esp_err_t wb_send_option_selected(wb_protocol_t *p, const char *option_id);

// 发送审批结果（confirm / cancel）
esp_err_t wb_send_answer(wb_protocol_t *p, const char *id, const char *value);

// 发送链路响应
esp_err_t wb_send_pong(wb_protocol_t *p);

// 发送通知
esp_err_t wb_send_notification(wb_protocol_t *p, const char *title, const char *body, const char *priority);

// 发送任务完成
esp_err_t wb_send_task_complete(wb_protocol_t *p, const char *task_name, const char *result);

// 发送任务失败
esp_err_t wb_send_task_error(wb_protocol_t *p, const char *task_name, const char *error);

// 发送注入文字确认
esp_err_t wb_send_inject_text(wb_protocol_t *p, const char *text, bool success);

// 发送按键事件（P3: Device → PC）
esp_err_t wb_send_key_event(wb_protocol_t *p, const char *button, const char *event);

// ============================================================
// 内部: BLE 层调用
// ============================================================

// 由 GATT 回调调用: 处理收到的 BLE 数据
void wb_protocol_on_rx_data(wb_protocol_t *p, const uint8_t *data, size_t len);

// 由连接管理调用: 通知连接状态变化
void wb_protocol_on_conn_change(wb_protocol_t *p, bool connected, uint16_t conn_handle);

// 检查是否可写 (需要连接状态)
bool wb_protocol_can_send(wb_protocol_t *p);

// 设置 GATT Notify 回调（由 BLE 层调用，签名与 esp_err_t 兼容）
typedef int (*wb_gatt_notify_fn)(uint16_t conn_handle, const uint8_t *data, size_t len);
void wb_protocol_set_gatt_notify_cb(wb_gatt_notify_fn cb);

#ifdef __cplusplus
}
#endif
