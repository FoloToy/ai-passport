// main/workbuddy_companion.c —— WorkBuddy 硬件伴侣（独立应用，状态屏 + 审批遥控器）
//
// 设计（满足二次开发强制重新设计规则，且是最轻的链路）：
//   - 设备是 WorkBuddy 的"无线状态屏 + 审批遥控器"。大脑永远是 PC 侧的 WorkBuddy。
//   - 传输层：USB-CDC 串口（ESP32-C3 原生 USB-Serial/JTAG 的 CDC = COM4）。
//     设备插 USB 即供电 + 通信，零配对、零扫描、不断连。协议为换行分隔 JSON。
//   - 屏幕：标题栏 + 状态药丸 + 可滚动消息记录 + 底部提示；审批时显示确认/拒绝对话框。
//   - 三键：OK 确认（对话框中）/ 翻看时滚动；UP/DOWN 在对话框中切换选项、空闲时滚动记录。
//   - 扬声器：仅做"叫你来看"的提示音（审批到达时响一声），设备不跑 STT/LLM/TTS。
//
// 硬件约束：ESP32-C3 无 PSRAM、无经典蓝牙、USB 是 Serial/JTAG（非 UAC/HID），
// 因此"语音对讲"在本板物理上不成立；状态屏 + 审批遥控器是与其能力匹配的最合理形态。

#include "workbuddy_companion.h"
#include "bsp_audio.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "wb_protocol.h"
#include "driver/usb_serial_jtag.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lvgl.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

// ============================================================
// 常量
// ============================================================

static const char *TAG = "wb_companion";

// 中文字体：tools/gen_font.py 用系统黑体生成的 16px 点阵（GB2312 全集 + ASCII + 符号，
// 共 8058 个码点，实测零缺字）。LVGL 自带的 source_han_sans_sc_16_cjk 只覆盖 1456 个码点，
// "确认/拒绝/说话/就绪"这些常用字都不在，屏幕上会画成方块 —— 这是上一版看不见内容的原因。
LV_FONT_DECLARE(lv_font_wb_zh_16);
#define F_BODY   (&lv_font_wb_zh_16)
#define F_SMALL  (&lv_font_wb_zh_16)
#define F_TITLE  (&lv_font_wb_zh_16)
// 生成字体的实测度量（tools/gen_font.py 里读出来的）
#define F_LINE_H   19
#define F_LINE_SP  3

// 音频参数（必须在使用前定义）
#define AUDIO_HZ          16000          // 采集 16kHz（ASR 质量优先）
#define AUDIO_FRAME_MS    50            // 50ms/帧 = 800 样本
#define AUDIO_FRAME_BYTES (AUDIO_HZ * 2 * AUDIO_FRAME_MS / 1000)   // 1600 字节原始 PCM
#define AUDIO_FRAME_SAMPLES (AUDIO_HZ * AUDIO_FRAME_MS / 1000)     // 800 样本
// 一块 = 4 字节 ADPCM 头 + 400 字节半字节流（800 样本，4:1）
#define AUDIO_ADPCM_BYTES (4 + AUDIO_FRAME_SAMPLES / 2)

// 播报（下行 TTS）
#define TTS_HZ_DEF        8000          // PC 端 TTS 统一转成 8k 单声道
#define TTS_RING_BYTES    (32 * 1024)   // 播放环形缓冲 = 2s @8k/16bit
#define TTS_WRITE_CHUNK   512           // 每次写 I2S 的字节数（32ms）

// 消息记录
#define MAX_LINES   80
#define LINE_LEN    72
#define LINE_UNITS  26            // 单行宽度（半角为单位，= 13 个汉字）
#define VIEW_ROWS   10            // 可见行数（224px / 22px）

// 屏幕（240x320，行高 19 + 行距 3 = 22px/行）
#define SCR_W   240
#define SCR_H   320
#define HEADER_H 30               // 顶部状态栏
#define STATUS_Y 32               // 状态药丸
#define STATUS_H 26
#define TRANS_Y  62               // 对话区起点
#define FOOTER_H 26               // 底部提示
#define TRANS_H  (SCR_H - FOOTER_H - 8 - TRANS_Y)     // 224 → 10 行

// 颜色（深色 AI 伴侣）
#define C_BG        0x0B0E14
#define C_HEADER    0x121826
#define C_CARD      0x1B2436
#define C_BORDER    0x2A3650
#define C_TEXT      0xE6EDF3
#define C_MUTED     0x8B98AD
#define C_ACCENT    0x4EA1FF
#define C_GREEN     0x3FB950
#define C_RED       0xF85149
#define C_YELLOW    0xD29922
#define C_PURPLE    0xBC8CFF

// 状态
typedef enum {
    ST_IDLE = 0,
    ST_THINKING,
    ST_NOTIFICATION,
    ST_OPTIONS,
} app_state_t;

// 选项
#define MAX_OPTS 5
#define OPT_LEN  48
static char      s_opts_buf[MAX_OPTS][OPT_LEN];
static const char *s_opts[MAX_OPTS];
static int  s_opt_cnt = 0;
static int  s_opt_sel = 0;
static lv_obj_t *s_opts_container = NULL;
static bool s_ask_active = false;     // 当前是否为审批对话框
static char s_ask_id[32];             // 审批 id（回传用）

// ============================================================
// 全局状态
// ============================================================

static bool        s_initialized = false;
static app_state_t  s_state = ST_IDLE;
static uint8_t      s_battery_pct = 0;

// 链路活性（借鉴官方 claude-buddy-port：PC 超过阈值不通信则判定离线）
#define PC_LIVENESS_MS  30000      // 30s 无消息 → 离线
static uint64_t     s_last_pc_seen_ms = 0;
static bool         s_offline = false;

static wb_protocol_t *s_wb = NULL;
static TaskHandle_t  s_rx_task = NULL;

// 对话采集（push-to-talk）：边录边发，设备不囤音频（C3 无 PSRAM，囤 10s 就要 80KB）
static bool         s_talking = false;
static TaskHandle_t s_audio_task = NULL;
static uint32_t     s_audio_frames = 0;            // 已上传的块数（诊断/结束判断）
static uint8_t      s_pcm_buf[AUDIO_FRAME_BYTES]; // 当前帧 PCM（全局区，1.6KB）
static char         s_batch_line[1200];            // 批量发送 JSON 缓冲（静态区，不压栈）
static uint64_t     s_ok_press_ms = 0;             // OK 按下时刻（区分"点一下"与"按住说话"）
static volatile int s_mic_level = 0;               // 采集电平 0..100（UI 用）

// 播报（PC → 设备扬声器）
static uint8_t           s_tts_ring[TTS_RING_BYTES];
static volatile size_t   s_tts_head = 0;      // 生产者（串口 RX 任务）写位置
static volatile size_t   s_tts_tail = 0;      // 消费者（播放任务）读位置
static volatile bool     s_tts_eof = false;   // 本次播报数据已发完
static volatile bool     s_tts_active = false;// 播放任务正在工作
static volatile int      s_tts_hz = TTS_HZ_DEF;
static TaskHandle_t      s_tts_task = NULL;
static SemaphoreHandle_t s_aud_mtx = NULL;    // 串行化 bsp_audio_set_format / write / read

// 消息记录
static char s_lines[MAX_LINES][LINE_LEN];
static int  s_line_cnt = 0;
static int  s_view_top = 0;

// LVGL
static lv_obj_t *s_scr = NULL;
static lv_obj_t *s_conn_dot = NULL;
static lv_obj_t *s_battery_bar = NULL;
static lv_obj_t *s_title = NULL;
static lv_obj_t *s_status_pill = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_trans_label = NULL;
static lv_obj_t *s_footer = NULL;
static lv_obj_t *s_level_box = NULL;   // 采集电平底板（仅聆听时显示）
static lv_obj_t *s_level_bar = NULL;
static lv_timer_t *s_timer = NULL;
static lv_timer_t *s_notif_timer = NULL;
static lv_obj_t *s_notif_overlay = NULL;
static lv_obj_t *s_notif_card = NULL;

// ============================================================
// Forward
// ============================================================

static int  serial_send(uint16_t conn, const uint8_t *data, size_t len);
static void serial_rx_task(void *arg);
static void play_beep(void);
static void show_ask(const char *id, const char *text);
static void on_ask(wb_protocol_t *p, const char *id, const char *text);
static void start_talk(void);
static void stop_talk(void);
static void audio_capture_task(void *arg);

// ============================================================
// LVGL 锁辅助
// ============================================================

static inline bool ui_lock(void)  { return bsp_lvgl_lock(500); }
static inline void ui_unlock(void) { bsp_lvgl_unlock(); }

// 底部提示语（240px 宽，16px 汉字约 13 字）
static const char *default_footer(void) { return "按住 OK 说话    ↑↓ 翻看"; }
static const char *dialog_footer(void)  { return "OK 确认   ↑↓ 选择"; }

// 像素块 / 文字（轻量，无图片依赖）
static lv_obj_t *px_block(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    return o;
}
static lv_obj_t *px_label(lv_obj_t *parent, int x, int y, const char *txt,
                          const lv_font_t *font, uint32_t color) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, txt);
    return l;
}

// ============================================================
// 消息记录
// ============================================================

// ============================================================
// 消息记录（UTF-8 宽度感知折行，汉字按 2 个单位算）
// ============================================================

static void ring_add(const char *s) {
    if (s_line_cnt < MAX_LINES) {
        snprintf(s_lines[s_line_cnt], LINE_LEN, "%s", s);
        s_line_cnt++;
    } else {
        memmove(s_lines, s_lines + 1, (MAX_LINES - 1) * LINE_LEN);
        snprintf(s_lines[MAX_LINES - 1], LINE_LEN, "%s", s);
    }
}

// 取一个 UTF-8 码点并前进 *pp（非法序列按单字节处理，不会越界）
static uint32_t utf8_next(const char **pp) {
    const uint8_t *p = (const uint8_t *)*pp;
    uint32_t cp = *p;
    int extra = 0;
    if (cp >= 0xF0)      { cp &= 0x07; extra = 3; }
    else if (cp >= 0xE0) { cp &= 0x0F; extra = 2; }
    else if (cp >= 0xC0) { cp &= 0x1F; extra = 1; }
    p++;
    for (int i = 0; i < extra; i++) {
        if ((*p & 0xC0) != 0x80) break;
        cp = (cp << 6) | (*p & 0x3F);
        p++;
    }
    *pp = (const char *)p;
    return cp;
}

// 显示宽度：ASCII 1，汉字/全角 2
static int cp_units(uint32_t cp) { return (cp < 0x80) ? 1 : 2; }

// 追加一段文本到记录（自动折行；首行带 who 前缀，续行缩进对齐）
static void append_line(const char *who, const char *text) {
    if (!text || !*text) return;

    char pre[10] = "", cont[10] = "";
    int pre_len = 0;
    if (who && *who) {
        pre_len = snprintf(pre, sizeof(pre), "%s ", who);
        if (pre_len > (int)sizeof(pre) - 2) pre_len = (int)sizeof(pre) - 2;
        memset(cont, ' ', (size_t)pre_len);
        cont[pre_len] = '\0';
    }

    char line[LINE_LEN];
    int n = 0, units = 0;
    bool has_content = false;
    const char *p = text;

    memcpy(line, pre, (size_t)pre_len);
    n = pre_len; units = pre_len;

    while (*p) {
        const char *q = p;
        uint32_t cp = utf8_next(&q);
        if (cp == '\n') {                    // 显式换行
            line[n] = '\0';
            ring_add(line);
            memcpy(line, cont, (size_t)pre_len);
            n = pre_len; units = pre_len; has_content = false;
            p = q;
            continue;
        }
        int u = cp_units(cp);
        if (units + u > LINE_UNITS) {         // 折行
            line[n] = '\0';
            ring_add(line);
            memcpy(line, cont, (size_t)pre_len);
            n = pre_len; units = pre_len; has_content = false;
        }
        int blen = (int)(q - p);
        if (n + blen < LINE_LEN - 1) {
            if (cp == '\r') { p = q; continue; }
            memcpy(line + n, p, (size_t)blen);
            n += blen; units += u; has_content = true;
        }
        p = q;
    }
    if (has_content || n > pre_len) {
        line[n] = '\0';
        ring_add(line);
    }

    s_view_top = s_line_cnt - VIEW_ROWS;
    if (s_view_top < 0) s_view_top = 0;
}

static void render_transcript(void) {
    if (!s_trans_label) return;
    static char buf[VIEW_ROWS * LINE_LEN + 32];
    buf[0] = '\0';
    int top = s_view_top;
    if (top > s_line_cnt - 1) top = s_line_cnt > 0 ? s_line_cnt - 1 : 0;
    for (int i = top; i < s_line_cnt && (i - top) < VIEW_ROWS; i++) {
        strncat(buf, s_lines[i], sizeof(buf) - strlen(buf) - 1);
        strncat(buf, "\n", sizeof(buf) - strlen(buf) - 1);
    }
    if (s_line_cnt == 0) strcpy(buf, "WorkBuddy 已连接\n按住 OK 说话，松开即发送");
    lv_label_set_text(s_trans_label, buf);
}

static void scroll_transcript(int delta) {
    s_view_top += delta;
    if (s_view_top < 0) s_view_top = 0;
    int max_top = s_line_cnt - VIEW_ROWS;
    if (max_top < 0) max_top = 0;
    if (s_view_top > max_top) s_view_top = max_top;
    render_transcript();
}

// ============================================================
// 状态 / UI 更新
// ============================================================

static void set_state(app_state_t st) {
    s_state = st;
    if (!s_status_label) return;
    if (s_talking) return;          // 采集态优先显示"聆听中"
    const char *txt = "就绪";
    uint32_t col = C_ACCENT;
    switch (st) {
        case ST_IDLE:         txt = "就绪";     col = C_ACCENT; break;
        case ST_THINKING:     txt = "思考中…";  col = C_YELLOW; break;
        case ST_NOTIFICATION: txt = "有提醒";   col = C_PURPLE; break;
        case ST_OPTIONS:      txt = "待确认";   col = C_RED;    break;
    }
    lv_label_set_text(s_status_label, txt);
    lv_obj_set_style_bg_color(s_status_pill, lv_color_hex(col), 0);
    lv_obj_set_style_bg_color(s_status_label, lv_color_hex(col), 0);
}

static void update_conn_dot(void) {
    if (s_conn_dot) lv_obj_set_style_bg_color(s_conn_dot,
        (s_wb && s_wb->connected) ? lv_color_hex(C_GREEN) : lv_color_hex(C_RED), 0);
}
static void update_battery(void) {
    if (!s_battery_bar) return;
    int pct = bsp_battery_soc();
    if (pct < 0) pct = s_battery_pct; else s_battery_pct = (uint8_t)pct;
    int w = (pct * 32) / 100; if (w < 1) w = 1;
    lv_obj_set_size(s_battery_bar, w, 8);
    lv_obj_set_style_bg_color(s_battery_bar, lv_color_hex(pct > 20 ? C_GREEN : C_RED), 0);
}

static void refresh_ui(void) {
    update_conn_dot();
    update_battery();
    set_state(s_state);
    render_transcript();
}

// ============================================================
// 通知覆盖层
// ============================================================

static void dismiss_notification(void);
static void notif_timer_cb(lv_timer_t *t) { (void)t; dismiss_notification(); }

static void show_notification(const char *title, const char *body, const char *priority) {
    if (s_notif_overlay) return;
    s_notif_overlay = lv_obj_create(s_scr);
    lv_obj_set_size(s_notif_overlay, SCR_W, SCR_H);
    lv_obj_set_style_bg_color(s_notif_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_notif_overlay, LV_OPA_60, 0);
    lv_obj_remove_flag(s_notif_overlay, LV_OBJ_FLAG_SCROLLABLE);

    s_notif_card = lv_obj_create(s_notif_overlay);
    lv_obj_set_size(s_notif_card, 208, 172);
    lv_obj_set_pos(s_notif_card, 16, 74);
    lv_obj_set_style_bg_color(s_notif_card, lv_color_hex(C_CARD), 0);
    lv_obj_set_style_radius(s_notif_card, 10, 0);
    lv_obj_set_style_border_width(s_notif_card, 2, 0);
    lv_obj_set_style_border_color(s_notif_card, lv_color_hex(C_PURPLE), 0);

    uint32_t bar = C_PURPLE;
    if (priority && (strcmp(priority, "high") == 0 || strcmp(priority, "urgent") == 0)) bar = C_RED;
    else if (priority && strcmp(priority, "low") == 0) bar = C_BORDER;
    px_block(s_notif_card, 0, 0, 200, 4, bar);

    lv_obj_t *tl = lv_label_create(s_notif_card);
    lv_obj_set_pos(tl, 12, 12);
    lv_obj_set_size(tl, 184, 24);
    lv_obj_set_style_text_font(tl, F_BODY, 0);
    lv_obj_set_style_text_color(tl, lv_color_hex(C_TEXT), 0);
    lv_label_set_text(tl, title ? title : "通知");

    lv_obj_t *bl = lv_label_create(s_notif_card);
    lv_obj_set_pos(bl, 12, 40);
    lv_obj_set_size(bl, 184, 92);
    lv_obj_set_style_text_font(bl, F_BODY, 0);
    lv_obj_set_style_text_color(bl, lv_color_hex(C_MUTED), 0);
    lv_obj_set_style_text_line_space(bl, F_LINE_SP, 0);
    lv_label_set_long_mode(bl, LV_LABEL_LONG_WRAP);
    lv_label_set_text(bl, body ? body : "");

    lv_obj_t *fl = lv_label_create(s_notif_card);
    lv_obj_set_pos(fl, 12, 142);
    lv_obj_set_style_text_font(fl, F_BODY, 0);
    lv_obj_set_style_text_color(fl, lv_color_hex(C_MUTED), 0);
    lv_label_set_text(fl, "OK 关闭");

    int auto_ms = 5000;
    if (priority && (strcmp(priority, "high") == 0 || strcmp(priority, "urgent") == 0)) auto_ms = 0;
    if (auto_ms > 0) s_notif_timer = lv_timer_create(notif_timer_cb, auto_ms, NULL);
}

static void dismiss_notification(void) {
    if (s_notif_timer) { lv_timer_delete(s_notif_timer); s_notif_timer = NULL; }
    if (s_notif_overlay) { lv_obj_del(s_notif_overlay); s_notif_overlay = NULL; s_notif_card = NULL; }
}

// ============================================================
// 选项 / 审批 UI
// ============================================================

// 从 JSON 解析选项文本到 s_opts_buf / s_opt_cnt
static void parse_options(const char *json) {
    const char *p = json;
    while ((p = strstr(p, "\"text\":")) && s_opt_cnt < MAX_OPTS) {
        p = strchr(p, ':'); if (!p) break; p++;
        while (*p == ' ' || *p == '"') p++;
        const char *end = p;
        while (*end && *end != '"' && *end != ',') end++;
        size_t len = (size_t)(end - p);
        if (len > 0 && len < OPT_LEN) {
            memcpy((char *)s_opts[s_opt_cnt], p, len);
            ((char *)s_opts[s_opt_cnt])[len] = '\0';
            s_opt_cnt++;
        }
        p = end;
    }
}

// 按当前 s_opt_cnt / s_opt_sel 重绘选项容器（解析与绘制分离，导航不会清空选项）
static void draw_option_rows(uint32_t sel_bg, uint32_t sel_fg) {
    if (s_opts_container) { lv_obj_del(s_opts_container); s_opts_container = NULL; }
    if (s_opt_cnt == 0) return;
    s_opts_container = lv_obj_create(s_scr);
    lv_obj_set_pos(s_opts_container, 10, TRANS_Y);
    lv_obj_set_size(s_opts_container, SCR_W - 20, TRANS_H);
    lv_obj_set_style_bg_color(s_opts_container,
        lv_color_hex(s_ask_active ? 0x241B2E : C_CARD), 0);
    lv_obj_set_style_radius(s_opts_container, 6, 0);
    lv_obj_remove_flag(s_opts_container, LV_OBJ_FLAG_SCROLLABLE);
    for (int i = 0; i < s_opt_cnt; i++) {
        int y = 8 + i * 40;
        uint32_t bg = (i == s_opt_sel) ? sel_bg : C_HEADER;
        uint32_t fg = (i == s_opt_sel) ? sel_fg : C_TEXT;
        lv_obj_t *row = lv_obj_create(s_opts_container);
        lv_obj_set_pos(row, 6, y);
        lv_obj_set_size(row, SCR_W - 20 - 12, 36);
        lv_obj_set_style_bg_color(row, lv_color_hex(bg), 0);
        lv_obj_set_style_radius(row, 4, 0);
        lv_obj_t *arrow = lv_label_create(row);
        lv_obj_set_pos(arrow, 6, 9);
        lv_obj_set_style_text_font(arrow, F_BODY, 0);
        lv_obj_set_style_text_color(arrow, lv_color_hex(fg), 0);
        lv_label_set_text(arrow, i == s_opt_sel ? ">" : " ");
        lv_obj_t *txt = lv_label_create(row);
        lv_obj_set_pos(txt, 24, 8);
        lv_obj_set_size(txt, SCR_W - 44, 22);
        lv_obj_set_style_text_font(txt, F_BODY, 0);
        lv_obj_set_style_text_color(txt, lv_color_hex(fg), 0);
        static char ob[64];
        const char *src = s_opts[i] ? s_opts[i] : "";
        // 截断按 UTF-8 字符数（11 个汉字 ≈ 22 单位）
        int units = 0, cut = 0;
        while (src[cut] && units < 22) {
            const char *q = src + cut;
            uint32_t cp = utf8_next(&q);
            int u = cp_units(cp);
            if (units + u > 22) break;
            units += u;
            cut = (int)(q - src);
        }
        if (src[cut]) {
            memcpy(ob, src, (size_t)cut);
            ob[cut] = '\0';
            strcat(ob, "…");
            src = ob;
        }
        lv_label_set_text(txt, src);
    }
}

static void show_options(const char *json) {
    if (s_opts_container) { lv_obj_del(s_opts_container); s_opts_container = NULL; }
    s_opt_cnt = 0; s_opt_sel = 0;
    for (int i = 0; i < MAX_OPTS; i++) s_opts[i] = s_opts_buf[i];
    parse_options(json);
    if (s_opt_cnt == 0) return;
    s_ask_active = false;
    s_state = ST_OPTIONS;
    draw_option_rows(C_ACCENT, C_BG);
}
static void clear_options(void) {
    s_opt_cnt = 0; s_opt_sel = 0; s_ask_active = false;
    if (s_opts_container) { lv_obj_del(s_opts_container); s_opts_container = NULL; }
}
static void nav_options(int d) {
    if (s_opt_cnt == 0) return;
    s_opt_sel = (s_opt_sel + d + s_opt_cnt) % s_opt_cnt;
    draw_option_rows(s_ask_active ? C_GREEN : C_ACCENT, C_BG);
}

// 审批对话框：固定两项 [确认 / 拒绝]
static void show_ask(const char *id, const char *text) {
    s_ask_active = true;
    s_opt_cnt = 2; s_opt_sel = 0;
    for (int i = 0; i < MAX_OPTS; i++) s_opts[i] = s_opts_buf[i];
    strncpy(s_opts_buf[0], "确认", OPT_LEN - 1); s_opts_buf[0][OPT_LEN - 1] = '\0';
    strncpy(s_opts_buf[1], "拒绝", OPT_LEN - 1); s_opts_buf[1][OPT_LEN - 1] = '\0';
    s_opts[0] = s_opts_buf[0]; s_opts[1] = s_opts_buf[1];
    if (id) { strncpy(s_ask_id, id, sizeof(s_ask_id) - 1); s_ask_id[sizeof(s_ask_id) - 1] = '\0'; }
    else s_ask_id[0] = '\0';

    // 记录 + 状态
    append_line("WB", text && *text ? text : "需要你确认");
    s_state = ST_OPTIONS;
    render_transcript();
    draw_option_rows(C_GREEN, C_BG);
    set_state(ST_OPTIONS);
    if (s_footer) lv_label_set_text(s_footer, dialog_footer());
    play_beep();
}

// 发送当前选择的审批结果
static void answer_current(void) {
    if (!s_wb) return;
    const char *v = (s_opt_sel == 0) ? "confirm" : "cancel";
    wb_send_answer(s_wb, s_ask_id, v);
    clear_options();
    s_state = ST_IDLE;
    if (ui_lock()) {
        if (s_footer) lv_label_set_text(s_footer, default_footer());
        refresh_ui();
        ui_unlock();
    }
}

// ============================================================
// 协议回调（来自串口 RX 任务 → 必须加 LVGL 锁）
// ============================================================

static void on_reply(wb_protocol_t *p, const char *text) {
    (void)p;
    if (!text) return;
    // 多会话时 PC 会在文本最前面加 ①②③ 标出这条属于哪个会话。
    // 拿它**代替** "WB" 前缀，而不是加在 WB 后面：
    //   "WB ②你：xxx"  →  "② 你：xxx"
    // 屏幕一行只有 26 units（13 个汉字），省下的 2 units 正好够多放一个字。
    // 单会话时没有标记，仍显示 "WB"，与以前完全一致。
    char who[8] = "WB";
    const char *body = text;
    const char *q = body;
    uint32_t cp = utf8_next(&q);
    if (cp >= 0x2460 && cp <= 0x2469) {          // ①-⑩
        int n = (int)(q - body);
        if (n > 0 && n < (int)sizeof(who)) {
            memcpy(who, body, (size_t)n);
            who[n] = '\0';
            body = q;
        }
    }
    if (!ui_lock()) return;
    append_line(who, body);
    ESP_LOGI(TAG, "回复入库 who=%s body_len=%u", who, (unsigned)strlen(body));
    set_state(ST_IDLE);
    render_transcript();
    ui_unlock();
}
static void on_notification(wb_protocol_t *p, const char *title, const char *body, const char *priority) {
    (void)p;
    if (priority && strcmp(priority, "status") == 0) {
        if (!ui_lock()) return;
        app_state_t st = ST_IDLE;
        if (title && strcmp(title, "thinking") == 0) st = ST_THINKING;
        else if (title && (strcmp(title, "waiting") == 0 || strcmp(title, "alert") == 0)) st = ST_NOTIFICATION;
        set_state(st);
        append_line("WB", body ? body : "");
        render_transcript();
        ui_unlock();
        return;
    }
    if (!ui_lock()) return;
    s_state = ST_NOTIFICATION;
    show_notification(title, body, priority);
    ui_unlock();
}
static void on_show_options(wb_protocol_t *p, const char *json) {
    (void)p;
    if (!ui_lock()) return;
    clear_options();
    show_options(json);
    s_state = ST_OPTIONS;
    ui_unlock();
}
static void on_ask(wb_protocol_t *p, const char *id, const char *text) {
    (void)p;
    if (s_talking) stop_talk();   // 审批到达先停麦，避免采集/提示音冲突
    if (!ui_lock()) return;
    show_ask(id, text);
    ui_unlock();
}
static void on_key_acked(wb_protocol_t *p, const char *button, const char *event, bool ok) {
    (void)p; (void)button; (void)event; (void)ok;
}
static void on_clear_screen(wb_protocol_t *p) {
    (void)p;
    if (!ui_lock()) return;
    dismiss_notification();
    clear_options();
    s_line_cnt = 0; s_view_top = 0;
    s_state = ST_IDLE;
    // 标题栏回到默认（PC 会在清屏后重发 wb_header 恢复会话标签）
    if (s_title) lv_label_set_text(s_title, "WorkBuddy");
    refresh_ui();
    ui_unlock();
}

// 链路活性心跳：刷新最后可见时间；若曾离线，恢复在线显示
static void on_heartbeat(wb_protocol_t *p) {
    (void)p;
    s_last_pc_seen_ms = esp_timer_get_time() / 1000;
    if (s_offline) {
        s_offline = false;
        if (ui_lock()) { update_conn_dot(); set_state(s_state); ui_unlock(); }
    }
}


// ============================================================
// 串口传输（USB-CDC / USB-Serial-JTAG）
// ============================================================

// wb_protocol 的"发送"回调：把一行 JSON 写到 USB-CDC。conn 参数在本传输层忽略。
//
// ⚠ 必须循环写：usb_serial_jtag_write_bytes() 在 TX 环形缓冲将满时会【部分写入】
//   并返回实际字节数。早先只调一次，音频包尾部的 `"}}` 被吃掉，PC 端整包 JSON
//   解析失败（还被误判成"CDC 吞吐只有 2KB/s"）。
static int serial_send(uint16_t conn, const uint8_t *data, size_t len) {
    (void)conn;
    if (!usb_serial_jtag_is_connected()) return -1;
    size_t off = 0;
    while (off < len) {
        int n = usb_serial_jtag_write_bytes(data + off, len - off, pdMS_TO_TICKS(200));
        if (n <= 0) return -1;
        off += (size_t)n;
    }
    return 0;
}

// 串口接收任务：读 CDC 字节流喂给协议解析器（协议层按 \n 分行）
static void serial_rx_task(void *arg) {
    (void)arg;
    uint8_t buf[128];
    ESP_LOGI(TAG, "串口接收任务启动 (USB-CDC)");
    for (;;) {
        int n = usb_serial_jtag_read_bytes(buf, sizeof(buf), pdMS_TO_TICKS(50));
        if (n > 0) {
            wb_protocol_on_rx_data(s_wb, buf, (size_t)n);
        }
        bool c = usb_serial_jtag_is_connected();
        if (s_wb && c != s_wb->connected) {
            s_wb->connected = c;
            wb_protocol_on_conn_change(s_wb, c, 0);
        }
    }
}

// 提示音：880Hz 短鸣，经 ES8311 扬声器
static void play_beep(void) {
    bsp_audio_init();
    if (bsp_audio_set_format(16000, 16, 1) != ESP_OK) return;
    bsp_audio_set_volume(60);
    const int hz = 880, dur_ms = 220;
    const int n = 16000 * dur_ms / 1000;
    int16_t *buf = malloc((size_t)n * sizeof(int16_t));
    if (!buf) return;
    const double pi2 = 2.0 * 3.141592653589793;
    for (int i = 0; i < n; i++) {
        double t = (double)i / 16000.0;
        double env = (i < n / 10) ? (double)i / (n / 10.0)
                                   : (i > n * 9 / 10 ? (double)(n - i) / (n / 10.0) : 1.0);
        buf[i] = (int16_t)(env * 3200.0 * sin(pi2 * hz * t));
    }
    bsp_audio_write(buf, (size_t)n * 2);
    free(buf);
}

// ============================================================
// 对话采集（push-to-talk）：卡片麦 → ADPCM 压缩 → 录完批量发送
// CDC 实测吞吐约 2KB/s（USB-Serial-JTAG 物理限制），流式不够。
// 故改为：录制中缓存音频 → 松手后批量发送全部帧 → PC 解码保存。
// 延迟 = 录制时长 + CDC 排空时长（可接受）。
// ============================================================

// 轻量 base64 编码（不依赖 mbedtls 组件）
static const char s_b64_tbl[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static size_t b64_encode(const uint8_t *src, size_t srclen, char *out, size_t outcap) {
    size_t i = 0, o = 0;
    if (outcap < 5) return 0;
    while (i + 3 <= srclen) {
        uint32_t v = ((uint32_t)src[i] << 16) | ((uint32_t)src[i+1] << 8) | src[i+2];
        out[o++] = s_b64_tbl[(v >> 18) & 63];
        out[o++] = s_b64_tbl[(v >> 12) & 63];
        out[o++] = s_b64_tbl[(v >> 6) & 63];
        out[o++] = s_b64_tbl[v & 63];
        i += 3;
    }
    size_t rem = srclen - i;
    if (rem == 1) {
        uint32_t v = (uint32_t)src[i] << 16;
        out[o++] = s_b64_tbl[(v >> 18) & 63];
        out[o++] = s_b64_tbl[(v >> 12) & 63];
        out[o++] = '='; out[o++] = '=';
    } else if (rem == 2) {
        uint32_t v = ((uint32_t)src[i] << 16) | ((uint32_t)src[i+1] << 8);
        out[o++] = s_b64_tbl[(v >> 18) & 63];
        out[o++] = s_b64_tbl[(v >> 12) & 63];
        out[o++] = s_b64_tbl[(v >> 6) & 63];
        out[o++] = '=';
    }
    if (o + 1 > outcap) return 0;
    out[o] = '\0';
    return o;
}

// ============================================================
// IMA ADPCM 编码器（16-bit PCM → 4-bit，压缩比 4:1）
// 兼容标准 WAV ima 格式；PC 端用 Python adpcm.decode() 解码。
// ============================================================

typedef struct {
    int16_t last;     // 预测值
    uint8_t step;     // 当前步长索引
} adpcm_enc_t;

// IMA ADPCM 步长表（值最大 32767，需要 uint16_t）
static const uint16_t adpcm_steps[89] = {
    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,
    50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,
    449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,
    3024,3327,3660,4026,4428,4871,5358,5894,6482,7132,7845,8630,9493,10442,11487,12635,13899,
    15289,16818,18500,20350,22385,24623,27086,29794,32767
};

// IMA ADPCM 步长索引调整表（编解码共用）
static const int8_t adpcm_idx_adj[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8
};

// 标准 IMA ADPCM 编码一个样点。重建过程必须与解码端逐位一致，否则会漂移。
static int adpcm_encode_sample(int16_t cur, int16_t *pred, uint8_t *step_idx) {
    int step = adpcm_steps[*step_idx];
    int delta = (int)cur - (int)*pred;
    uint8_t code = 0;
    if (delta < 0) { code = 8; delta = -delta; }
    int diff = 0;
    if (delta >= step)              { code |= 4; delta -= step;      diff += step; }
    if (delta >= (step >> 1))       { code |= 2; delta -= step >> 1; diff += step >> 1; }
    if (delta >= (step >> 2))       { code |= 1;                     diff += step >> 2; }
    diff += step >> 3;
    if (code & 8) diff = -diff;
    int v = (int)*pred + diff;
    if (v > 32767) v = 32767; else if (v < -32768) v = -32768;
    *pred = (int16_t)v;
    int idx = (int)*step_idx + adpcm_idx_adj[code & 7];
    if (idx < 0) idx = 0; else if (idx > 88) idx = 88;
    *step_idx = (uint8_t)idx;
    return code & 0x0F;
}

// 编码器的预测器/步长跨帧连续（每 50ms 重置会在帧边界制造爆音）
static adpcm_enc_t s_enc = { .last = 0, .step = 0 };
static void adpcm_encoder_reset(void) { s_enc.last = 0; s_enc.step = 0; }

// 编码一块 PCM → [vp_lo, vp_hi, step_idx, 0] + 4bit 半字节流。
// 头部写的是【本块起始】状态，PC 端逐块独立解码，不依赖网络丢包后的历史。
static void adpcm_encode_block(const int16_t *src, size_t n_samples, uint8_t *dst) {
    dst[0] = (uint8_t)(s_enc.last & 0xFF);
    dst[1] = (uint8_t)((s_enc.last >> 8) & 0xFF);
    dst[2] = s_enc.step;
    dst[3] = 0;
    size_t di = 4;
    for (size_t i = 0; i < n_samples; i += 2) {
        uint8_t c0 = (uint8_t)adpcm_encode_sample(src[i], &s_enc.last, &s_enc.step);
        uint8_t c1 = (i + 1 < n_samples)
                     ? (uint8_t)adpcm_encode_sample(src[i + 1], &s_enc.last, &s_enc.step) : 0;
        dst[di++] = (uint8_t)((c0 << 4) | (c1 & 0x0F));
    }
}

// ============================================================
// 扬声器播报：PC 把 TTS 音频（8k/16bit/单声道，IMA ADPCM 压缩）推过来，
// 这里解码进环形缓冲，由播放任务按实时速率喂给 ES8311。
// 环形缓冲 = 2s 音频；PC 端按"领先 1.2s"的节奏发包，不会溢出。
// ============================================================

// 解码一个播出分片：[vp_lo, vp_hi, step_idx, _] + 4bit 半字节流
static int adpcm_decode_block(const uint8_t *in, size_t n, int16_t *out, size_t outcap) {
    if (n < 5) return 0;
    int vp = (int16_t)((uint16_t)in[0] | ((uint16_t)in[1] << 8));
    int si = in[2];
    if (si > 88) si = 88;
    size_t o = 0;
    for (size_t i = 4; i < n; i++) {
        for (int half = 0; half < 2; half++) {
            int code = half ? (in[i] & 0x0F) : (in[i] >> 4);
            int step = adpcm_steps[si];
            int delta = step >> 3;
            if (code & 4) delta += step;
            if (code & 2) delta += step >> 1;
            if (code & 1) delta += step >> 2;
            if (code & 8) delta = -delta;
            vp += delta;
            if (vp > 32767) vp = 32767;
            else if (vp < -32768) vp = -32768;
            if (o < outcap) out[o++] = (int16_t)vp;
            si += adpcm_idx_adj[code & 7];
            if (si < 0) si = 0;
            else if (si > 88) si = 88;
        }
    }
    return (int)o;
}

static size_t tts_free_bytes(void) {
    size_t head = s_tts_head, tail = s_tts_tail;
    size_t used = (head + TTS_RING_BYTES - tail) % TTS_RING_BYTES;
    return TTS_RING_BYTES - 1 - used;
}

// 往环形缓冲写 PCM（生产者：串口 RX 任务）
static void tts_ring_write(const int16_t *pcm, size_t samples) {
    size_t bytes = samples * 2;
    if (bytes > tts_free_bytes()) {
        static uint32_t last_warn = 0;
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000000);
        if (now - last_warn > 2) { last_warn = now; ESP_LOGW(TAG, "播报缓冲满，丢 %u 字节", (unsigned)bytes); }
        return;
    }
    const uint8_t *src = (const uint8_t *)pcm;
    for (size_t i = 0; i < bytes; i++) {
        s_tts_ring[s_tts_head] = src[i];
        s_tts_head = (s_tts_head + 1) % TTS_RING_BYTES;
    }
}

// 播放任务：环形缓冲里有数据就往 I2S 写；一次播报排空后关掉 codec 让位给采集
static void tts_play_task(void *arg) {
    (void)arg;
    int16_t buf[TTS_WRITE_CHUNK / 2];
    bool format_open = false;
    ESP_LOGI(TAG, "播报任务启动");
    for (;;) {
        if (!s_tts_active) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }

        size_t used = (s_tts_head + TTS_RING_BYTES - s_tts_tail) % TTS_RING_BYTES;
        if (used < sizeof(buf)) {
            if (s_tts_eof) {                 // 数据放完 → 本次播报结束
                s_tts_active = false;
                s_tts_eof = false;
                format_open = false;         // 让下一次播报重新设格式
                if (ui_lock()) { set_state(s_state); ui_unlock(); }
            } else {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            continue;
        }

        if (!format_open) {
            if (s_aud_mtx) xSemaphoreTake(s_aud_mtx, portMAX_DELAY);
            bsp_audio_init();
            esp_err_t e = bsp_audio_set_format((uint32_t)s_tts_hz, 16, 1);
            if (s_aud_mtx) xSemaphoreGive(s_aud_mtx);
            if (e != ESP_OK) { ESP_LOGW(TAG, "播报设格式失败 %s", esp_err_to_name(e)); s_tts_active = false; continue; }
            bsp_audio_set_volume(75);
            format_open = true;
        }

        size_t n = sizeof(buf);
        if (n > used) n = used;
        for (size_t i = 0; i < n; i++) {
            buf[i / 2] = 0;
        }
        uint8_t *dst = (uint8_t *)buf;
        for (size_t i = 0; i < n; i++) {
            dst[i] = s_tts_ring[s_tts_tail];
            s_tts_tail = (s_tts_tail + 1) % TTS_RING_BYTES;
        }
        if (s_aud_mtx) xSemaphoreTake(s_aud_mtx, portMAX_DELAY);
        bsp_audio_write(buf, n);
        if (s_aud_mtx) xSemaphoreGive(s_aud_mtx);
    }
}

static void tts_stop(void) {
    s_tts_active = false;
    s_tts_eof = false;
    s_tts_head = s_tts_tail = 0;
}

// ---- 协议回调：PC 端播报 ----
static void on_tts_begin(wb_protocol_t *p, int hz, bool adpcm) {
    (void)p; (void)adpcm;
    if (hz < 8000 || hz > 48000) hz = TTS_HZ_DEF;
    if (s_talking) stop_talk();          // 说话时不要播报
    s_tts_hz = hz;
    s_tts_head = s_tts_tail = 0;
    s_tts_eof = false;
    s_tts_active = true;
    ESP_LOGI(TAG, "开始播报 hz=%d", hz);
    if (ui_lock()) {
        if (s_status_label) lv_label_set_text(s_status_label, "播报中…");
        if (s_status_pill) lv_obj_set_style_bg_color(s_status_pill, lv_color_hex(C_PURPLE), 0);
        ui_unlock();
    }
}

static void on_tts_chunk(wb_protocol_t *p, int seq, const uint8_t *data, size_t len) {
    (void)p; (void)seq;
    if (!s_tts_active || !data || len == 0) return;
    static int16_t pcm[AUDIO_FRAME_BYTES];        // 800 样本足够一包
    int samples = 0;
    if (len >= 4 && len <= 600) {
        samples = adpcm_decode_block(data, len, pcm, sizeof(pcm) / sizeof(pcm[0]));
    } else {
        // 纯 PCM 兜底
        samples = (int)(len / 2);
        if ((size_t)samples > sizeof(pcm) / sizeof(pcm[0])) samples = sizeof(pcm) / sizeof(pcm[0]);
        memcpy(pcm, data, (size_t)samples * 2);
    }
    if (samples > 0) tts_ring_write(pcm, (size_t)samples);
}

static void on_tts_end(wb_protocol_t *p) {
    (void)p;
    s_tts_eof = true;      // 由播放任务把残留数据放完再收尾
}
static void on_tts_stop(wb_protocol_t *p) {
    (void)p;
    tts_stop();
}

// ---- 协议回调：标题栏（多会话时显示会话标签，如 "WB② 2会话"）----
// WorkBuddy 是多标签单窗口，所有会话共用一块屏；PC 侧给每个会话分配
// ①②③ 标签并标在每条消息前面，这里只负责把"当前是几个会话"写到标题栏。
static void on_header(wb_protocol_t *p, const char *text) {
    (void)p;
    if (!text) return;
    ESP_LOGI(TAG, "标题栏 → %s", text);
    if (!ui_lock()) return;
    if (s_title) lv_label_set_text(s_title, text);
    ui_unlock();
}

// ============================================================
// 远程调试：PC 发 wb_debug_talk 开始/停止采集（与 OK 键 push-to-talk 同效）
static void on_debug_talk(wb_protocol_t *p, bool on) {
    (void)p;
    if (on) start_talk(); else stop_talk();
}

// 运行时改麦克风输入增益（不重烧固件就能调，削顶时往下调）
static void on_mic_gain(wb_protocol_t *p, float db) {
    (void)p;
    if (s_aud_mtx) xSemaphoreTake(s_aud_mtx, portMAX_DELAY);
    bsp_audio_init();
    esp_err_t e = bsp_audio_set_in_gain(db);
    if (s_aud_mtx) xSemaphoreGive(s_aud_mtx);
    ESP_LOGI(TAG, "麦克风增益 -> %.1f dB (%s)", db, esp_err_to_name(e));
}

// 上传一块音频：[seq(1)][len(2 LE)][ADPCM 头 4B + 半字节流]
static void send_audio_block(const uint8_t *block, size_t len) {
    static uint8_t frame_buf[640];
    static char b64[900];
    size_t total = len + 3;
    if (total > sizeof(frame_buf)) return;
    frame_buf[0] = (uint8_t)(s_audio_frames & 0xFF);
    frame_buf[1] = (uint8_t)(len & 0xFF);
    frame_buf[2] = (uint8_t)((len >> 8) & 0xFF);
    memcpy(frame_buf + 3, block, len);
    size_t olen = b64_encode(frame_buf, total, b64, sizeof(b64));
    if (olen == 0) return;
    int n = snprintf(s_batch_line, sizeof(s_batch_line),
                     "{\"type\":\"wb_audio\",\"payload\":{\"s\":%u,\"b\":\"",
                     (unsigned)(s_audio_frames & 0xFF));
    if (n <= 0 || (size_t)n + olen + 5 > sizeof(s_batch_line)) return;
    memcpy(s_batch_line + n, b64, olen);
    n += (int)olen;
    s_batch_line[n++] = '"';   // 结束 base64 字符串
    s_batch_line[n++] = '}';   // 结束 payload 对象
    s_batch_line[n++] = '}';   // 结束根对象
    s_batch_line[n++] = '\n';
    if (serial_send(0, (const uint8_t *)s_batch_line, (size_t)n) == 0) s_audio_frames++;
}

static void audio_capture_task(void *arg) {
    (void)arg;
    if (s_aud_mtx) xSemaphoreTake(s_aud_mtx, portMAX_DELAY);
    bsp_audio_init();
    esp_err_t fe = bsp_audio_set_format(AUDIO_HZ, 16, 1);
    if (s_aud_mtx) xSemaphoreGive(s_aud_mtx);
    if (fe != ESP_OK) {
        ESP_LOGW(TAG, "采集设格式失败 %s", esp_err_to_name(fe));
        s_talking = false;
        s_audio_task = NULL;
        vTaskDelete(NULL);
    }
    // 不要在这里设输出音量：esp_codec_dev_set_out_vol(0) 会动 ES8311 的输出链路，
    // 和厂商 demo（录完再设音量）不一致。采集期间本来也听不到自己的声音。
    // 给 codec 一点稳定时间再读，否则头几帧是废数据。
    vTaskDelay(pdMS_TO_TICKS(60));
    adpcm_encoder_reset();
    s_audio_frames = 0;
    ESP_LOGI(TAG, "开始采集（16k/16/mono，边录边发）");
    int frame = 0;
    while (s_talking) {
        if (s_aud_mtx) xSemaphoreTake(s_aud_mtx, portMAX_DELAY);
        int rc = bsp_audio_read(s_pcm_buf, AUDIO_FRAME_BYTES);
        if (s_aud_mtx) xSemaphoreGive(s_aud_mtx);
        if (rc != ESP_OK) { vTaskDelay(pdMS_TO_TICKS(2)); continue; }

        // 电平（每 4 帧更新一次，供 UI 画音量条）
        if ((frame++ & 3) == 0) {
            const int16_t *s = (const int16_t *)s_pcm_buf;
            int64_t acc = 0;
            for (int i = 0; i < AUDIO_FRAME_SAMPLES; i++) acc += (int32_t)s[i] * s[i];
            float rms = sqrtf((float)(acc / (int64_t)AUDIO_FRAME_SAMPLES));
            int lv = (int)(rms / 60.0f);
            s_mic_level = lv > 100 ? 100 : lv;
        }

        uint8_t block[AUDIO_ADPCM_BYTES];
        adpcm_encode_block((const int16_t *)s_pcm_buf, AUDIO_FRAME_SAMPLES, block);
        send_audio_block(block, AUDIO_ADPCM_BYTES);
    }
    s_mic_level = 0;
    ESP_LOGI(TAG, "采集结束：共上传 %u 块（约 %.1fs）",
             (unsigned)s_audio_frames, (double)s_audio_frames * AUDIO_FRAME_MS / 1000.0);
    s_audio_task = NULL;
    vTaskDelete(NULL);
}

static void start_talk(void) {
    if (s_talking) return;
    if (s_tts_active) tts_stop();      // 播报让位给采集
    s_talking = true;
    s_audio_frames = 0;
    s_mic_level = 0;
    if (ui_lock()) {
        if (s_status_label) lv_label_set_text(s_status_label, "聆听中…");
        if (s_status_pill) lv_obj_set_style_bg_color(s_status_pill, lv_color_hex(C_RED), 0);
        if (s_footer) lv_label_set_text(s_footer, "松开 OK 结束录音");
        if (s_level_box) lv_obj_clear_flag(s_level_box, LV_OBJ_FLAG_HIDDEN);
        ui_unlock();
    }
    xTaskCreatePinnedToCore(audio_capture_task, "wb_audio", 5120, NULL, 6, &s_audio_task, PRO_CPU_NUM);
}

static void stop_talk(void) {
    if (!s_talking) return;
    s_talking = false;
    if (ui_lock()) {
        if (s_level_box) lv_obj_add_flag(s_level_box, LV_OBJ_FLAG_HIDDEN);
        if (s_footer) lv_label_set_text(s_footer, default_footer());
        set_state(s_state);
        ui_unlock();
    }
    while (s_audio_task != NULL) vTaskDelay(pdMS_TO_TICKS(5));
    if (s_audio_frames > 0) {
        static const char end_msg[] = "{\"type\":\"wb_audio_end\",\"payload\":{}}\n";
        serial_send(0, (const uint8_t *)end_msg, sizeof(end_msg) - 1);
    }
}

// ============================================================
// 公开 API
// ============================================================

esp_err_t workbuddy_companion_start(void) {
    if (s_initialized) return ESP_ERR_INVALID_STATE;
    s_initialized = true;
    s_line_cnt = 0; s_view_top = 0; s_state = ST_IDLE;
    s_ask_active = false; s_opt_cnt = 0; s_opt_sel = 0;

    s_wb = wb_protocol_new();
    if (!s_wb) return ESP_ERR_NO_MEM;
    wb_protocol_set_gatt_notify_cb(serial_send);
    s_wb->on_show_options  = on_show_options;
    s_wb->on_ask          = on_ask;
    s_wb->on_notification = on_notification;
    s_wb->on_reply        = on_reply;
    s_wb->on_key_acked    = on_key_acked;
    s_wb->on_clear_screen = on_clear_screen;
    s_wb->on_heartbeat    = on_heartbeat;
    s_wb->on_debug_talk   = on_debug_talk;
    s_wb->on_mic_gain     = on_mic_gain;
    s_wb->on_tts_begin    = on_tts_begin;
    s_wb->on_tts_chunk    = on_tts_chunk;
    s_wb->on_tts_end      = on_tts_end;
    s_wb->on_header       = on_header;
    s_wb->connected = usb_serial_jtag_is_connected();
    s_last_pc_seen_ms = esp_timer_get_time() / 1000;  // 启动即给足 30s 宽限
    s_offline = false;

    // 音频总线互斥（采集与播报共用同一个 codec/I2S）
    if (!s_aud_mtx) s_aud_mtx = xSemaphoreCreateMutex();
    // 播报任务常驻（空闲时 20ms 轮询，几乎不占 CPU）
    if (!s_tts_task) {
        xTaskCreatePinnedToCore(tts_play_task, "wb_tts", 4096, NULL, 5, &s_tts_task, PRO_CPU_NUM);
    }

    // 安装 USJ 驱动（若 console 已占用则忽略 ESP_FAIL，直接使用已安装的驱动）
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    cfg.tx_buffer_size = 8192; cfg.rx_buffer_size = 4096;
    esp_err_t e = usb_serial_jtag_driver_install(&cfg);
    if (e != ESP_OK) ESP_LOGI(TAG, "USJ 驱动已安装(console 占用)，忽略: %s", esp_err_to_name(e));

    if (xTaskCreatePinnedToCore(serial_rx_task, "wb_uart", 8192, NULL,
                                 5, &s_rx_task, PRO_CPU_NUM) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "伴侣(串口)启动完成");
    return ESP_OK;
}

esp_err_t workbuddy_companion_stop(void) {
    s_initialized = false;
    if (s_rx_task) { vTaskDelete(s_rx_task); s_rx_task = NULL; }
    if (s_wb) { wb_protocol_free(s_wb); s_wb = NULL; }
    return ESP_OK;
}

// ============================================================
// LVGL 定时（状态动画）
// ============================================================

static void tick(lv_timer_t *t) {
    (void)t;
    if (!s_initialized) return;

    // 链路活性检测：USB 物理在线，但 PC 超过阈值未通信 → 逻辑离线
    uint64_t now_ms = esp_timer_get_time() / 1000;
    if (s_wb && s_wb->connected && !s_offline &&
        (now_ms - s_last_pc_seen_ms) > PC_LIVENESS_MS) {
        s_offline = true;
        // 离线时清除敏感的待审审批态（防误判批准）
        if (s_ask_active) { clear_options(); s_state = ST_IDLE; }
        if (ui_lock()) {
            if (s_conn_dot) lv_obj_set_style_bg_color(s_conn_dot, lv_color_hex(C_RED), 0);
            if (s_status_label) lv_label_set_text(s_status_label, "PC 离线");
            if (s_status_pill) lv_obj_set_style_bg_color(s_status_pill, lv_color_hex(C_RED), 0);
            ui_unlock();
        }
        ESP_LOGW(TAG, "PC 链路超时，进入离线显示");
    }

    static int phase = 0;
    if (s_state == ST_THINKING) {
        phase = (phase + 1) % 3;
        uint32_t c = (phase == 0) ? C_YELLOW : (phase == 1 ? C_GREEN : C_ACCENT);
        lv_obj_set_style_bg_color(s_status_pill, lv_color_hex(c), 0);
    } else if (s_state == ST_OPTIONS) {
        phase ^= 1;
        lv_obj_set_style_bg_color(s_status_pill,
            phase ? lv_color_hex(C_RED) : lv_color_hex(C_HEADER), 0);
    }
    if (s_talking && s_level_box && s_level_bar) {
        int w = (s_mic_level * (SCR_W - 20)) / 100;
        if (w < 2) w = 2;
        lv_obj_set_size(s_level_bar, w, 5);
    }
    update_battery();
}

// ============================================================
// 应用页面
// ============================================================

void workbuddy_companion_enter(void) {
    s_scr = lv_obj_create(NULL);
    lv_obj_set_size(s_scr, SCR_W, SCR_H);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(C_BG), 0);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    // 标题栏
    lv_obj_t *header = px_block(s_scr, 0, 0, SCR_W, HEADER_H, C_HEADER);
    (void)header;
    s_conn_dot = lv_obj_create(header);
    lv_obj_set_pos(s_conn_dot, 10, 10);
    lv_obj_set_size(s_conn_dot, 10, 10);
    lv_obj_set_style_radius(s_conn_dot, 5, 0);
    lv_obj_set_style_bg_color(s_conn_dot, lv_color_hex(C_RED), 0);
    lv_obj_set_style_border_width(s_conn_dot, 0, 0);
    s_title = px_label(header, 26, 5, "WorkBuddy", F_BODY, C_TEXT);
    // 多会话时 PC 会用 wb_header 改写它（如 "WB② 2会话"）；单会话保持 WorkBuddy
    // 电池
    px_block(header, SCR_W - 50, 9, 32, 12, C_BORDER);
    px_block(header, SCR_W - 54, 12, 5, 6, C_BORDER);
    s_battery_bar = px_block(header, SCR_W - 48, 11, 28, 8, C_GREEN);

    // 状态药丸
    s_status_pill = px_block(s_scr, 10, STATUS_Y, SCR_W - 20, STATUS_H, C_ACCENT);
    s_status_label = px_label(s_status_pill, 10, 3, "就绪", F_BODY, C_BG);

    // 消息记录
    s_trans_label = lv_label_create(s_scr);
    lv_obj_set_pos(s_trans_label, 10, TRANS_Y);
    lv_obj_set_size(s_trans_label, SCR_W - 20, TRANS_H);
    lv_obj_set_style_text_font(s_trans_label, F_BODY, 0);
    lv_obj_set_style_text_color(s_trans_label, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_line_space(s_trans_label, F_LINE_SP, 0);
    lv_label_set_long_mode(s_trans_label, LV_LABEL_LONG_WRAP);
    render_transcript();

    // 采集电平条（仅聆听时可见）
    s_level_box = px_block(s_scr, 10, SCR_H - FOOTER_H - 6, SCR_W - 20, 5, C_BORDER);
    s_level_bar = px_block(s_level_box, 0, 0, 2, 5, C_RED);
    lv_obj_add_flag(s_level_box, LV_OBJ_FLAG_HIDDEN);

    // 底部提示
    s_footer = px_label(s_scr, 0, SCR_H - FOOTER_H + 3, default_footer(), F_BODY, C_MUTED);
    lv_obj_set_align(s_footer, LV_ALIGN_TOP_MID);

    s_timer = lv_timer_create(tick, 400, NULL);

    s_battery_pct = (uint8_t)bsp_battery_soc();
    if (s_battery_pct == 0) s_battery_pct = 85;
    refresh_ui();
    lv_screen_load(s_scr);
}

void workbuddy_companion_exit(void) {
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    dismiss_notification();
    clear_options();
    if (s_scr) { lv_obj_delete(s_scr); s_scr = NULL; }
    s_conn_dot = s_battery_bar = s_title = NULL;
    s_status_pill = s_status_label = s_trans_label = s_footer = NULL;
    s_opts_container = NULL;
}

// ============================================================
// 按键（来自 button 组件，已入队）
// ============================================================

// 把按键上送给 PC（PC 端转成 上/下/回车 键，直接操作 WorkBuddy 界面）
static void send_key(const char *name) {
    if (!s_wb || !s_wb->connected) return;
    wb_send_key_event(s_wb, name, "click");
}

// 上报"本次按下要做什么"。PC 端据此打日志：
// 没有这条 = 按键没触发；有这条但没音频 = 采音失败。两种修法完全不同。
static void send_key_ev(const char *name, const char *ev) {
    if (!s_wb || !s_wb->connected) return;
    wb_send_key_event(s_wb, name, ev);
}

// 放弃这次采集（点一下 OK 的情形：不足 450ms，当作回车而非说话）
static void cancel_talk(void) {
    s_talking = false;
    int guard = 0;
    while (s_audio_task != NULL && guard++ < 200) vTaskDelay(pdMS_TO_TICKS(5));
    s_audio_frames = 0;
    s_mic_level = 0;
    if (ui_lock()) {
        if (s_level_box) lv_obj_add_flag(s_level_box, LV_OBJ_FLAG_HIDDEN);
        if (s_footer) lv_label_set_text(s_footer, default_footer());
        set_state(s_state);
        ui_unlock();
    }
}

void workbuddy_companion_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    const bool dialog = s_ask_active || s_opt_cnt > 0;

    // ---- OK：按住说话，松手发送；点一下 = 回车 ----
    if (btn == BSP_BTN_OK && ev == BSP_BTN_PRESS) {
        s_ok_press_ms = (uint64_t)(esp_timer_get_time() / 1000);
        // 通知层不能挡住"说话"：先把通知关掉，再当作一次正常按下继续往下走。
        // 之前是直接 return（只关通知）——结果每轮 AI 结束都会弹通知，
        // 通知一直挡着 OK，用户按 OK 只是在关通知，永远录不到音。
        if (s_notif_overlay) {
            dismiss_notification();
            s_state = ST_IDLE;
            if (ui_lock()) { refresh_ui(); ui_unlock(); }
        }
        // 对话/审批态里 OK 只表示"确认"，不抢麦克风
        if (dialog) return;
        if (s_tts_active) { tts_stop(); return; }  // 正在播报：按一下先停播报
        send_key_ev("ok", "press");
        start_talk();
        return;
    }
    if (btn == BSP_BTN_OK && ev == BSP_BTN_RELEASE && s_talking) {
        uint64_t held = (uint64_t)(esp_timer_get_time() / 1000) - s_ok_press_ms;
        send_key_ev("ok", "release");
        if (held < 450) {
            cancel_talk();
            send_key("ok");
        } else {
            stop_talk();
        }
        return;
    }
    if (ev != BSP_BTN_CLICK) return;

    // ---- 通知层：OK 关闭 ----
    if (s_notif_overlay && btn == BSP_BTN_OK) {
        dismiss_notification();
        s_state = ST_IDLE;
        if (ui_lock()) { refresh_ui(); ui_unlock(); }
        return;
    }

    // ---- 审批对话框 / 选项：设备本地选择，结果经 wb_answer 回传 ----
    if (dialog) {
        if (btn == BSP_BTN_UP) nav_options(-1);
        else if (btn == BSP_BTN_DOWN) nav_options(+1);
        else if (btn == BSP_BTN_OK) answer_current();
        return;
    }

    // ---- 空闲：UP/DOWN 翻看记录，同时把这颗键交给 PC 操作 WorkBuddy ----
    if (btn == BSP_BTN_UP)   { send_key("up");   scroll_transcript(-1); }
    else if (btn == BSP_BTN_DOWN) { send_key("down"); scroll_transcript(+1); }
}
