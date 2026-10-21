// main/demo_ble.c —— NimBLE GAP+GATT 服务器，支持 WorkBuddy BLE 协议
//
// 能力:
//   - GAP广播: 广播 FoloPassport 设备名
//   - GATT服务: Nordic UART Service (NUS)
//     * TX Characteristic (写): 接收 PC Bridge 命令 (JSON)
//     * RX Characteristic (通知): 向 PC Bridge 发送消息
//   - 语音录音: 长按 OK 开始录音，松手发送 ASR 结果
//
// BLE UUID (与 PC Bridge 保持一致):
//   Service:  6e400001-b5a3-f393-e0a9-e50e24dcca9e
//   TX Char: 6e400002-b5a3-f393-e0a9-e50e24dcca9e  (写)
//   RX Char: 6e400003-b5a3-f393-e0a9-e50e24dcca9e  (通知)

#include "demo.h"
#include "demo_radio.h"
#include "ui_pixel.h"
#include "bsp_audio.h"
#include "bsp_button.h"
#include "wb_protocol.h"

#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nimble/nimble_port.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "lvgl.h"

#include <string.h>

static const char *TAG = "demo_ble";

// ============================================================
// 常量
// ============================================================

#define DEVICE_NAME       "FoloPassport"
#define BLE_STOP_TIMEOUT_MS 2000

// GATT handle (由 NimBLE 自动分配)
static uint16_t s_rx_char_handle;  // TX Characteristic (PC 写 → ESP)
// static uint16_t s_tx_char_handle;  // RX Characteristic (ESP 通知 → PC) — 暂不存
static uint16_t s_svc_handle;

// 连接状态
static bool s_gatt_connected = false;
static uint16_t s_conn_handle;

// 语音录音
static bool s_recording = false;
static TaskHandle_t s_rec_task_handle;

// LVGL
static lv_obj_t *s_scr;
static lv_obj_t *s_status;
static lv_obj_t *s_rec_indicator;
static lv_timer_t *s_timer;
static SemaphoreHandle_t s_host_stopped;
static TaskHandle_t s_host_task;
static bool s_initialized = false;
static bool s_start_requested = false;
static bool s_stop_in_progress = false;
static bool s_host_done = false;

// 协议实例
static wb_protocol_t *s_wb = NULL;

// ============================================================
// 选项 UI
// ============================================================

#define MAX_OPTIONS 5
static const char *s_options[MAX_OPTIONS];
static int s_option_count = 0;
static int s_option_selected = 0;

static void show_options_ui(const char *options_json);
static void clear_options_ui(void);
static void on_key_acked(wb_protocol_t *p, const char *button, const char *event, bool success);
static void on_clear_screen(wb_protocol_t *p);

// ============================================================
// 工具
// ============================================================

// BLE 地址类型 (GAP 广播需要)
static uint8_t s_addr_type;

static esp_err_t advertise(void);

// 发送 GATT Notify（通过 NimBLE）
// ESP-IDF 5.5.3 / NimBLE 1.6.0: 使用 ble_hs_mbuf_att_pkt() 分配 ATT mbuf
static int gatt_notify_impl(uint16_t conn_handle, const uint8_t *data, size_t len) {
    if (!s_gatt_connected || s_rx_char_handle == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    struct os_mbuf *om = ble_hs_mbuf_att_pkt();
    if (!om) return ESP_ERR_NO_MEM;

    int rc = os_mbuf_append(om, data, len);
    if (rc != 0) {
        os_mbuf_free_chain(om);
        return ESP_ERR_NO_MEM;
    }

    rc = ble_gattc_notify_custom(conn_handle, s_rx_char_handle, om);
    if (rc != 0) {
        ESP_LOGW(TAG, "GATT notify 失败: %d", rc);
    }
    return rc == 0 ? ESP_OK : ESP_FAIL;
}

// ============================================================
// GATT 服务定义 (静态定义，ESP-IDF 5.5.3 API)
// 旧版 gatt_svcs 已移除，改用动态注册
// ============================================================

// 旧版静态服务定义已移除（ESP-IDF 5.5.3 改用动态注册）
static void gatt_svr_on_register(uint16_t handle, uint16_t attr_handle,
                                  int status, void *arg) {
    (void)arg;
    if (status == 0) {
        ESP_LOGI(TAG, "GATT 服务注册成功: handle=%d", handle);
        s_svc_handle = handle;
    } else {
        ESP_LOGE(TAG, "GATT 服务注册失败: %d", status);
    }
}

// ============================================================
// GATT 回调 (ESP-IDF 5.5.3 / NimBLE 1.6.0)
// 写请求通过 GATT access 回调处理（不在 GAP event 中）
// ============================================================

static int gatt_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg) {
    (void)arg;
    (void)conn_handle;

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        // 数据在 ctxt->om 中
        struct os_mbuf *om = ctxt->om;
        if (om && OS_MBUF_PKTLEN(om) > 0) {
            uint8_t buf[512];
            uint16_t len = OS_MBUF_PKTLEN(om);
            if (len > sizeof(buf) - 1) len = sizeof(buf) - 1;
            os_mbuf_copydata(om, 0, len, buf);
            buf[len] = '\0';
            ESP_LOGD(TAG, "BLE 写数据: attr=%d len=%d", attr_handle, len);
            wb_protocol_on_rx_data(s_wb, buf, len);
        }
        return 0;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        return 0;
    }

    return 0;
}

static int gap_event(struct ble_gap_event *event, void *arg);

// ============================================================
// GAP / GATT 事件
// ============================================================

static int gap_event(struct ble_gap_event *event, void *arg) {
    (void)arg;

    switch (event->type) {

        case BLE_GAP_EVENT_CONNECT: {
            s_gatt_connected = true;
            s_conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "BLE 连接建立: conn_handle=%d", s_conn_handle);
            wb_protocol_on_conn_change(s_wb, true, s_conn_handle);

            // 在已连接状态下停止广播（节省功耗）
            ble_gap_adv_stop();

            // 更新 UI
            if (s_status) {
                lv_label_set_text(s_status, "已连接\n\n等待 PC Bridge...\n\n长按 OK 开始录音\n短按 OK 查看状态");
            }
            break;
        }

        case BLE_GAP_EVENT_DISCONNECT: {
            s_gatt_connected = false;
            s_conn_handle = 0;
            ESP_LOGI(TAG, "BLE 连接断开");
            wb_protocol_on_conn_change(s_wb, false, 0);

            // 清理录音状态
            if (s_recording) {
                s_recording = false;
                if (s_rec_task_handle) {
                    vTaskDelete(s_rec_task_handle);
                    s_rec_task_handle = NULL;
                }
            }

            // 清除选项
            clear_options_ui();

            // 重新广播
            if (s_start_requested) {
                advertise();
            }

            // 更新 UI
            if (s_status) {
                lv_label_set_text(s_status, "广播中\n\nName: FoloPassport\n\n等待连接...\n\nOK: 重启广播");
            }
            break;
        }

        case BLE_GAP_EVENT_ADV_COMPLETE:
            if (s_start_requested && !s_gatt_connected) {
                int rc = advertise();
                if (rc != 0) {
                    ESP_LOGE(TAG, "重启广播失败: %d", rc);
                }
            }
            break;

        // PC 订阅/取消订阅通知
        case BLE_GAP_EVENT_SUBSCRIBE: {
            ESP_LOGI(TAG, "GATT 订阅事件: reason=%d notify=%d->%d indicate=%d->%d attr=%d",
                     event->subscribe.reason,
                     event->subscribe.prev_notify,
                     event->subscribe.cur_notify,
                     event->subscribe.prev_indicate,
                     event->subscribe.cur_indicate,
                     event->subscribe.attr_handle);
            break;
        }

        default:
            break;
    }
    return 0;
}

// ============================================================
// 广播
// ============================================================

static int advertise(void) {
    struct ble_hs_adv_fields fields = { 0 };
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (const uint8_t *)DEVICE_NAME;
    fields.name_len = strlen(DEVICE_NAME);
    fields.name_is_complete = 1;

    // 宣告支持 WorkBuddy NUS Service
    static const ble_uuid16_t nus_svc_uuid = BLE_UUID16_INIT(0x7800);
    fields.mfg_data = (uint8_t *)&nus_svc_uuid;
    fields.mfg_data_len = 2;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) return rc;

    struct ble_gap_adv_params params = { 0 };
    params.conn_mode = BLE_GAP_CONN_MODE_UND;  // 可发现 + 可连接
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    params.itvl_min = BLE_GAP_ADV_ITVL_MS(100);  // 100ms
    params.itvl_max = BLE_GAP_ADV_ITVL_MS(100);  // ESP-IDF 5.5.3: use BLE_GAP_ADV_ITVL_MS()

    rc = ble_gap_adv_start(s_addr_type, NULL, BLE_HS_FOREVER, &params, gap_event, NULL);
    return rc;
}

// ============================================================
// GATT 服务注册（ESP-IDF 5.5.3 / NimBLE 1.6.0 动态注册）
// ============================================================

static void register_gatt_service(void) {
    int rc;

    // NUS Service: 6e400001-b5a3-f393-e0a9-e50e24dcca9e
    // TX Char (写):  6e400002-b5a3-f393-e0a9-e50e24dcca9e
    // RX Char (通知): 6e400003-b5a3-f393-e0a9-e50e24dcca9e

    // BLE UUID 声明（BLE_UUID128_INIT 用于静态初始化，不可用于 static const 聚合体）
    // NUS Service:  6e400001-b5a3-f393-e0a9-e50e24dcca9e
    // TX Char (写): 6e400002-b5a3-f393-e0a9-e50e24dcca9e
    // RX Char (通知): 6e400003-b5a3-f393-e0a9-e50e24dcca9e
    static const ble_uuid128_t ble_svc_uuid   = BLE_UUID128_INIT(
        0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
        0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x00, 0x6e
    );
    static const ble_uuid128_t ble_tx_uuid    = BLE_UUID128_INIT(
        0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
        0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x00, 0x6e
    );
    static const ble_uuid128_t ble_rx_uuid    = BLE_UUID128_INIT(
        0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
        0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x00, 0x6e
    );

    // TX Characteristic (写，无响应)
    static struct ble_gatt_chr_def chr_defs[] = {
        {
            .uuid = (const ble_uuid_t *)&ble_tx_uuid,
            .access_cb = gatt_access_cb,
            .flags = BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE,
            .min_key_size = 16,
            .val_handle = &s_rx_char_handle,  // filled by NimBLE after registration
        },
        {
            .uuid = (const ble_uuid_t *)&ble_rx_uuid,
            .access_cb = gatt_access_cb,
            .flags = BLE_GATT_CHR_F_NOTIFY,
            .min_key_size = 16,
        },
        { 0 },  // terminator
    };

    // 声明 service
    static const struct ble_gatt_svc_def gatt_svcs[] = {
        {
            .type = BLE_GATT_SVC_TYPE_PRIMARY,
            .uuid = (const ble_uuid_t *)&ble_svc_uuid,
            .characteristics = chr_defs,
        },
        { 0 },  // terminator
    };

    // 注册 GATT 服务
    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs 失败: %d", rc);
    }
}

// ============================================================
// 协议回调实现
// ============================================================

static void on_voice_text(wb_protocol_t *p, const char *text) {
    (void)p;
    if (!text) {
        // 开始录音
        ESP_LOGI(TAG, "协议回调: 开始录音");
    } else if (text[0] == '\0') {
        // 结束录音（触发 ASR 已由录音任务处理）
    } else {
        // 注入文字确认，显示在屏幕上
        ESP_LOGI(TAG, "注入文字: %s", text);
        if (s_status && !s_recording) {
            char buf[256];
            snprintf(buf, sizeof(buf), "已注入\n\n%s\n\nOK: 重新录音", text);
            lv_label_set_text(s_status, buf);
        }
    }
}

static void on_show_options(wb_protocol_t *p, const char *options_json) {
    (void)p;
    ESP_LOGI(TAG, "显示选项: %s", options_json);
    show_options_ui(options_json);
}

static void on_option_selected(wb_protocol_t *p, const char *option_id) {
    (void)p;
    ESP_LOGI(TAG, "选项被选中: %s", option_id);
    wb_send_option_selected(s_wb, option_id);
    clear_options_ui();
}

static void on_notification(wb_protocol_t *p, const char *title, const char *body, const char *priority) {
    (void)p; (void)priority;
    ESP_LOGI(TAG, "通知: %s - %s", title, body);
    if (s_status) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s\n\n%s", title, body);
        lv_label_set_text(s_status, buf);
    }
}

// P3: 按键 ACK（PC 已处理按键）
static void on_key_acked(wb_protocol_t *p, const char *button, const char *event, bool success) {
    (void)p;
    ESP_LOGI(TAG, "按键已 ACK: %s/%s success=%d", button, event, success);
    // 可选：屏幕指示 ACK 状态（短暂显示）
}

// P3: 清屏（WorkBuddy 新任务开始）
static void on_clear_screen(wb_protocol_t *p) {
    (void)p;
    ESP_LOGI(TAG, "清屏");
    clear_options_ui();
    if (s_status) {
        lv_label_set_text(s_status, "处理中...\n\n请稍候...");
    }
}

// ============================================================
// 语音录音任务
// ============================================================

#define PCM_CHUNK_SIZE  2048   // 每次读取 2048 字节 (~64ms@16kHz)
#define AUDIO_HZ        16000
#define AUDIO_BITS      16
#define AUDIO_CH        1

// 录音缓冲（最多 30 秒 = 960000 字节）
#define MAX_AUDIO_SECS  30
#define MAX_AUDIO_BYTES (AUDIO_HZ * AUDIO_BITS / 8 * AUDIO_CH * MAX_AUDIO_SECS)

static uint8_t *s_audio_buf = NULL;
static size_t   s_audio_len  = 0;
static size_t   s_audio_cap  = 0;

static void audio_rec_task(void *arg) {
    (void)arg;
    ESP_LOGI(TAG, "录音任务启动");
    vTaskDelay(pdMS_TO_TICKS(100)); // 等待显示刷新

    // 配置音频格式
    esp_err_t err = bsp_audio_set_format(AUDIO_HZ, AUDIO_BITS, AUDIO_CH);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "音频格式设置失败: %s", esp_err_to_name(err));
        s_recording = false;
        vTaskDelete(NULL);
        return;
    }

    // 分配录音缓冲
    if (!s_audio_buf) {
        s_audio_cap = MAX_AUDIO_BYTES;
        s_audio_buf = malloc(s_audio_cap);
    }
    s_audio_len = 0;

    // 更新 UI
    if (s_rec_indicator) {
        lv_label_set_text(s_rec_indicator, "🔴 录音中...\n\n松开发送");
    }
    if (s_status) {
        lv_label_set_text(s_status, "🔴 录音中\n\n请说话...\n\n松开发送");
    }

    uint8_t pcm[PCM_CHUNK_SIZE];
    ESP_LOGI(TAG, "开始 PCM 采集...");

    while (s_recording && s_audio_len + PCM_CHUNK_SIZE < s_audio_cap) {
        int rc = bsp_audio_read(pcm, sizeof(pcm));
        if (rc == ESP_OK) {
            memcpy(s_audio_buf + s_audio_len, pcm, PCM_CHUNK_SIZE);
            s_audio_len += PCM_CHUNK_SIZE;
        } else {
            ESP_LOGW(TAG, "bsp_audio_read 返回: %s", esp_err_to_name(rc));
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    ESP_LOGI(TAG, "录音结束: %d bytes (%.1fs)", s_audio_len,
             (double)s_audio_len / (AUDIO_HZ * AUDIO_BITS / 8));

    // 更新 UI
    if (s_rec_indicator) {
        lv_label_set_text(s_rec_indicator, "⏳ 识别中...");
    }
    if (s_status) {
        lv_label_set_text(s_status, "⏳ 发送音频\n请稍候...");
    }

    // 发送音频数据（BLE 分包）
    // 格式: {"type":"wb_voice_end","payload":{"audio_data":[...],"duration":N}}\n
    if (s_audio_len > 0 && s_gatt_connected) {
        // 分包发送，每包 ≤ WB_TX_MAX_CHUNK 字节
        // 先发送包数信息
        char meta[128];
        snprintf(meta, sizeof(meta),
                 "{\"type\":\"wb_voice_end\",\"payload\":{\"total_bytes\":%zu,\"duration\":%.1f}}\n",
                 s_audio_len, (double)s_audio_len / (AUDIO_HZ * AUDIO_BITS / 8));
        gatt_notify_impl(s_conn_handle, (const uint8_t *)meta, strlen(meta));
        vTaskDelay(pdMS_TO_TICKS(50));

        // 发送实际音频数据（Base64 编码）
        // 简化方案: 直接将 PCM 字节转为十六进制字符串分批发
        // 每包发送 N 个字节的十六进制表示
        const size_t hex_bytes_per_chunk = 200; // 产生 ~400 字符
        char hex_pkt[512];
        size_t sent = 0;
        int seq = 0;

        while (sent < s_audio_len) {
            size_t chunk = s_audio_len - sent;
            if (chunk > hex_bytes_per_chunk) chunk = hex_bytes_per_chunk;

            // 转 hex
            char hex[hex_bytes_per_chunk * 2 + 4];
            for (size_t i = 0; i < chunk; i++) {
                snprintf(hex + i * 2, 3, "%02x", s_audio_buf[sent + i]);
            }

            snprintf(hex_pkt, sizeof(hex_pkt),
                     "{\"type\":\"wb_voice_chunk\",\"payload\":{\"seq\":%d,\"hex\":\"%s\"}}\n",
                     seq++, hex);

            gatt_notify_impl(s_conn_handle, (const uint8_t *)hex_pkt, strlen(hex_pkt));
            sent += chunk;
            vTaskDelay(pdMS_TO_TICKS(30)); // 让 BLE 有时间发送
        }

        ESP_LOGI(TAG, "音频发送完成: %d bytes, %d 包", s_audio_len, seq);
    } else if (s_audio_len == 0) {
        ESP_LOGW(TAG, "没有录音数据");
        wb_send_voice_text(s_wb, "（无音频）");
    } else {
        ESP_LOGW(TAG, "GATT 未连接，无法发送音频");
    }

    // 重置状态
    s_audio_len = 0;
    s_recording = false;

    // 更新 UI
    if (s_rec_indicator) {
        lv_label_set_text(s_rec_indicator, "");
        lv_obj_add_flag(s_rec_indicator, LV_OBJ_FLAG_HIDDEN);
    }

    // 发回 ASR 占位结果（PC 端 Whisper ASR 后会发回 wb_voice_text）
    // 这里只是 UI 反馈
    if (s_status) {
        lv_label_set_text(s_status, "已发送\n\n等待识别结果...\n\n长按录音");
    }

    s_rec_task_handle = NULL;
    vTaskDelete(NULL);
}

// 切换录音状态
static void toggle_recording(void) {
    if (s_recording) {
        // 停止录音
        s_recording = false;
        ESP_LOGI(TAG, "停止录音");
    } else {
        // 开始录音
        if (!s_gatt_connected) {
            ESP_LOGW(TAG, "GATT 未连接，无法录音发送");
            if (s_status) {
                lv_label_set_text(s_status, "⚠️ 未连接\n\n请先配对 PC Bridge");
            }
            return;
        }
        if (!bsp_audio_init()) {
            ESP_LOGE(TAG, "音频初始化失败");
            return;
        }
        s_recording = true;
        ESP_LOGI(TAG, "开始录音");
        xTaskCreatePinnedToCore(&audio_rec_task, "audio_rec",
                                4096, NULL, 5, &s_rec_task_handle,
                                PRO_CPU_NUM);  // ESP32-C3 is single-core
    }
}

// ============================================================
// 选项 UI
// ============================================================

static void show_options_ui(const char *options_json) {
    // 解析选项 JSON（简化：提取 "text" 字段）
    clear_options_ui();

    // 简单解析: {"options":[{"text":"继续"},{"text":"停止"}]}
    const char *p = options_json;
    s_option_count = 0;

    while ((p = strstr(p, "\"text\":")) && s_option_count < MAX_OPTIONS) {
        p = strchr(p, ':');
        if (!p) break;
        p++;
        while (*p == ' ' || *p == '"') p++;
        const char *end = p;
        while (*end && *end != '"' && *end != ',') end++;
        size_t len = (size_t)(end - p);
        if (len > 0) {
            char *dst = (char *)s_options[s_option_count];
            if (dst && len < 64) {
                memcpy(dst, p, len);
                dst[len] = '\0';
                s_option_count++;
            } else if (dst && len >= 64) {
                // 超长截断
                memcpy(dst, p, 62);
                dst[62] = '\0';
                s_option_count++;
            }
        }
        p = end;
    }

    if (s_option_count == 0) {
        ESP_LOGW(TAG, "无法解析选项 JSON: %s", options_json);
        return;
    }

    s_option_selected = 0;

    // 更新屏幕显示（通过状态标签）
    if (s_status) {
        char buf[256] = "选项:\n";
        for (int i = 0; i < s_option_count; i++) {
            char line[80];
            snprintf(line, sizeof(line), "%s %s\n",
                     i == s_option_selected ? "→" : "  ",
                     s_options[i] ? s_options[i] : "");
            strncat(buf, line, sizeof(buf) - strlen(buf) - 1);
        }
        strncat(buf, "\n↑↓ 选择，OK 确认", sizeof(buf) - strlen(buf) - 1);
        lv_label_set_text(s_status, buf);
    }
}

static void clear_options_ui(void) {
    s_option_count = 0;
    s_option_selected = 0;
    if (s_status) {
        lv_label_set_text(s_status, "已连接\n\n长按 OK 开始录音\n短按 OK 查看状态");
    }
}

// 选项导航
static void options_nav(int delta) {
    if (s_option_count == 0) return;
    s_option_selected = (s_option_selected + delta + s_option_count) % s_option_count;
    show_options_ui("{}"); // 复用解析逻辑
}

// ============================================================
// 选项选中（确认）
static void confirm_option(void) {
    if (s_option_count == 0) return;
    const char *selected = s_options[s_option_selected];
    ESP_LOGI(TAG, "确认选项: %s", selected);

    // 发送选项
    char id[16];
    snprintf(id, sizeof(id), "%d", s_option_selected + 1);
    wb_send_option_selected(s_wb, id);

    // 更新 UI
    if (s_status) {
        char buf[128];
        snprintf(buf, sizeof(buf), "已选择:\n→ %s", selected ? selected : "");
        lv_label_set_text(s_status, buf);
    }
    s_option_count = 0;
}

// ============================================================
// NimBLE Host 生命周期
// ============================================================

static void on_reset(int reason) {
    ESP_LOGE(TAG, "NimBLE reset: reason=%d", reason);
}

static void on_sync(void) {
    int rc = ble_hs_util_ensure_addr(0);
    if (rc == 0) rc = ble_hs_id_infer_auto(0, &s_addr_type);
    if (rc == 0 && s_start_requested) {
        // 注册 GATT 服务
        register_gatt_service();
        rc = advertise();
    }
    if (rc != 0) {
        ESP_LOGE(TAG, "on_sync 失败: %d", rc);
    }
}

static void host_task(void *arg) {
    (void)arg;
    nimble_port_run();
    xSemaphoreGive(s_host_stopped);
    for (;;) vTaskSuspend(NULL);
}

// ============================================================
// 公开 API (demo.h 接口)
// ============================================================

esp_err_t demo_ble_start(void) {
    if (s_initialized) return ESP_ERR_INVALID_STATE;

    esp_err_t err = demo_radio_nvs_prepare();
    if (err != ESP_OK) return err;

    err = nimble_port_init();
    if (err != ESP_OK) return err;

    s_initialized = true;
    s_stop_in_progress = false;
    s_host_done = false;
    s_gatt_connected = false;
    s_recording = false;

    // 创建协议实例
    s_wb = wb_protocol_new();
    if (!s_wb) return ESP_ERR_NO_MEM;
    wb_protocol_set_user_data(s_wb, NULL);
    wb_protocol_set_gatt_notify_cb(gatt_notify_impl);

    s_wb->on_voice_text = on_voice_text;
    s_wb->on_show_options = on_show_options;
    s_wb->on_option_selected = on_option_selected;
    s_wb->on_notification = on_notification;
    s_wb->on_key_acked = on_key_acked;    // P3
    s_wb->on_clear_screen = on_clear_screen; // P3

    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(TAG, "device_name_set 失败: %d", rc);
        return ESP_FAIL;
    }

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;

    s_host_stopped = xSemaphoreCreateBinary();
    if (!s_host_stopped) return ESP_ERR_NO_MEM;

    s_start_requested = true;
    if (xTaskCreatePinnedToCore(host_task, "nimble_host", NIMBLE_HS_STACK_SIZE,
                                NULL, configMAX_PRIORITIES - 4, &s_host_task,
                                NIMBLE_CORE) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t demo_ble_stop(void) {
    s_start_requested = false;

    // 停止录音
    if (s_recording) {
        s_recording = false;
        if (s_rec_task_handle) {
            vTaskDelete(s_rec_task_handle);
            s_rec_task_handle = NULL;
        }
    }

    if (!s_initialized) return ESP_OK;

    if (s_host_task && !s_stop_in_progress) {
        (void)ble_gap_adv_stop();
        int rc = nimble_port_stop();
        if (rc != 0) {
            ESP_LOGE(TAG, "nimble_port_stop 失败: %d", rc);
            return ESP_FAIL;
        }
        s_stop_in_progress = true;
    }

    if (s_host_task && !s_host_done) {
        if (!s_host_stopped ||
            xSemaphoreTake(s_host_stopped, pdMS_TO_TICKS(BLE_STOP_TIMEOUT_MS)) != pdTRUE) {
            ESP_LOGE(TAG, "等待 NimBLE host 停止超时");
            return ESP_ERR_TIMEOUT;
        }
        s_host_done = true;
    }

    if (s_host_task) {
        vTaskDelete(s_host_task);
        s_host_task = NULL;
    }

    esp_err_t err = nimble_port_deinit();
    if (err != ESP_OK) ESP_LOGE(TAG, "nimble_port_deinit: %s", esp_err_to_name(err));

    // 清理协议
    if (s_wb) {
        wb_protocol_free(s_wb);
        s_wb = NULL;
    }

    // 清理音频缓冲
    if (s_audio_buf) {
        free(s_audio_buf);
        s_audio_buf = NULL;
    }

    s_initialized = false;
    s_stop_in_progress = false;
    s_gatt_connected = false;
    s_host_done = false;
    if (s_host_stopped) {
        vSemaphoreDelete(s_host_stopped);
        s_host_stopped = NULL;
    }
    return ESP_OK;
}

// ============================================================
// LVGL 页面
// ============================================================

static void tick(lv_timer_t *timer) {
    (void)timer;
    if (!s_initialized) return;

    if (s_recording && s_rec_indicator && !lv_obj_has_flag(s_rec_indicator, LV_OBJ_FLAG_HIDDEN)) {
        // 录音指示器闪烁
        static int blink = 0;
        blink ^= 1;
        lv_obj_set_style_text_color(s_rec_indicator,
            blink ? lv_color_hex(0xFF2020) : lv_color_hex(0xFF8080), 0);
    }
}

void demo_ble_enter(void) {
    s_scr = ui_pixel_screen_create("BLUETOOTH LE");
    lv_obj_t *panel = ui_pixel_panel_create(s_scr, 22, 50, 196, 170, UI_PAPER);

    s_status = lv_label_create(panel);
    lv_obj_set_width(s_status, 168);
    lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_status, lv_color_hex(UI_INK), 0);
    lv_obj_center(s_status);

    // 录音指示器（顶部）
    s_rec_indicator = lv_label_create(s_scr);
    lv_label_set_text(s_rec_indicator, "");
    lv_obj_align(s_rec_indicator, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_add_flag(s_rec_indicator, LV_OBJ_FLAG_HIDDEN);

    ui_pixel_mascot_create(s_scr, 101, 244);

    if (!s_initialized) {
        lv_label_set_text(s_status, "启动 NimBLE...\n\n按 OK 继续");
    } else if (s_gatt_connected) {
        lv_label_set_text(s_status, "已连接\n\n长按 OK 开始录音\n短按 OK 查看状态");
    } else {
        lv_label_set_text(s_status, "广播中\n\nName: FoloPassport\n\n等待连接...\n\nOK: 重启广播");
    }

    s_timer = lv_timer_create(tick, 500, NULL);
    lv_screen_load(s_scr);
}

void demo_ble_exit(void) {
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_recording) {
        s_recording = false;
        if (s_rec_task_handle) {
            vTaskDelete(s_rec_task_handle);
            s_rec_task_handle = NULL;
        }
    }
    if (s_scr) { lv_obj_delete(s_scr); s_scr = NULL; }
    s_status = NULL;
    s_rec_indicator = NULL;
    s_option_count = 0;
}

void demo_ble_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    // 选项导航
    if (s_option_count > 0) {
        if (btn == BSP_BTN_UP && ev == BSP_BTN_CLICK) {
            options_nav(-1);
            wb_send_key_event(s_wb, "up", "press");   // P3: 通知 PC
            return;
        }
        if (btn == BSP_BTN_DOWN && ev == BSP_BTN_CLICK) {
            options_nav(+1);
            wb_send_key_event(s_wb, "down", "press"); // P3
            return;
        }
        if (btn == BSP_BTN_OK && ev == BSP_BTN_CLICK) {
            confirm_option();
            wb_send_key_event(s_wb, "ok", "press");  // P3
            return;
        }
    }

    // 录音控制（仅 OK 键）
    if (btn == BSP_BTN_OK) {
        if (ev == BSP_BTN_PRESS) {
            // 按下开始录音
            if (!s_recording && s_gatt_connected) {
                toggle_recording();
            } else if (!s_gatt_connected) {
                ESP_LOGW(TAG, "GATT 未连接");
            }
        } else if (ev == BSP_BTN_RELEASE && s_recording) {
            // 松手停止录音并发送
            toggle_recording();
        } else if (ev == BSP_BTN_CLICK) {
            // 短按：显示状态
            if (s_status) {
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "状态: %s\n\n连接: %s\n\n录音: %s\n\n长按 OK 录音",
                         s_initialized ? "就绪" : "未初始化",
                         s_gatt_connected ? "已连接" : "未连接",
                         s_recording ? "录音中" : "空闲");
                lv_label_set_text(s_status, buf);
            }
        }
    }

    // P3: 非选项模式下的上下键也转发（供 WorkBuddy 弹窗控制用）
    if (!s_recording && s_option_count == 0) {
        if (btn == BSP_BTN_UP && ev == BSP_BTN_CLICK) {
            wb_send_key_event(s_wb, "up", "press");
        } else if (btn == BSP_BTN_DOWN && ev == BSP_BTN_CLICK) {
            wb_send_key_event(s_wb, "down", "press");
        }
        // OK 长按 = 录音已在上面处理，此处只转发短按确认
    }

    // 重新广播（广播状态 + OK 短按）
    if (btn == BSP_BTN_OK && ev == BSP_BTN_CLICK &&
        s_initialized && !s_gatt_connected) {
        int rc = advertise();
        if (rc != 0) {
            ESP_LOGE(TAG, "广播重启失败: %d", rc);
        }
    }
}
