#include "demo.h"

#include "bsp_audio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "rtttl_player.h"
#include "tetris_model.h"

#define COLOR_BG       0x070A11
#define COLOR_PANEL    0x101724
#define COLOR_BORDER   0x2A3850
#define COLOR_TEXT     0xEAF3FF
#define COLOR_DIM      0x7F92AA
#define COLOR_ACCENT   0x31D7F2

#define CELL_PITCH     12
#define CELL_SIZE      10
#define BOARD_CANVAS_W (TETRIS_BOARD_WIDTH * CELL_PITCH)
#define BOARD_CANVAS_H (TETRIS_BOARD_HEIGHT * CELL_PITCH)
#define NEXT_CANVAS_W  (4 * CELL_PITCH)
#define NEXT_CANVAS_H  (4 * CELL_PITCH)
#define BURST_CANVAS_W  64
#define BURST_CANVAS_H  64
#define BURST_PIXEL     4

#define MIC_SAMPLE_RATE        16000
#define MIC_FRAME_SAMPLES      256
#define MIC_CALIBRATION_FRAMES 32
#define MIC_REARM_FRAMES       12
#define MIC_COOLDOWN_MS        900
#define MIC_SPEAKER_GUARD_MS   180
#define MIC_BUTTON_GUARD_MS    250
#define MIC_NOISE_FLOOR        180
#define MIC_MIN_AVG_LEVEL      650
#define MIC_MIN_PEAK_LEVEL     3000
#define MIC_TASK_STACK_BYTES   4096
#define BURST_VISIBLE_MS       260

static const char *TAG = "tetris";
static const char *SFX_START =
    "start:d=16,o=5,b=168:c,e,g,c6,8p,g6";
static const char *SFX_KEY =
    "key:d=32,o=7,b=320:c";
static const char *SFX_LAND =
    "land:d=32,o=4,b=240:c,16p";
static const char *SFX_LINE_CLEAR =
    "clear:d=32,o=6,b=240:16c5,p,e,g,c7,e7,16g7";
static const char *SFX_GAME_OVER =
    "gameover:d=8,o=5,b=100:g,f,e,d,4c";
static const uint32_t PIECE_COLORS[] = {
    COLOR_BG, 0x31D7F2, 0xFFD84A, 0xB56BFF,
    0x46E46F, 0xFF4A57, 0x4E77FF, 0xFF9D32,
    0x344256, 0xD9F4FF, 0x151C28,
};

typedef struct {
    bsp_btn_t btn;
    bsp_btn_ev_t ev;
    bool mic_drop;
} tetris_input_t;

typedef struct {
    tetris_piece_t piece;
    uint8_t rotation;
    int8_t x;
    int8_t y;
    int8_t ghost_y;
    bool valid;
} tetris_visual_t;

LV_DRAW_BUF_DEFINE_STATIC(board_buf, BOARD_CANVAS_W, BOARD_CANVAS_H, LV_COLOR_FORMAT_I4);
LV_DRAW_BUF_DEFINE_STATIC(next_buf, NEXT_CANVAS_W, NEXT_CANVAS_H, LV_COLOR_FORMAT_I4);
LV_DRAW_BUF_DEFINE_STATIC(burst_buf, BURST_CANVAS_W, BURST_CANVAS_H, LV_COLOR_FORMAT_I4);

static tetris_model_t s_model;
static lv_obj_t *s_scr;
static lv_obj_t *s_board_canvas;
static lv_obj_t *s_next_canvas;
static lv_obj_t *s_stats_label;
static lv_obj_t *s_status_label;
static lv_obj_t *s_input_label;
static lv_obj_t *s_burst_canvas;
static lv_timer_t *s_timer;
static QueueHandle_t s_input_queue;
static uint64_t s_last_drop_ms;
static uint64_t s_last_press_ms[3];
static bool s_press_seen[3];
static tetris_visual_t s_visual;
static bool s_up_press_shifted;
static TaskHandle_t s_mic_task;
static volatile bool s_mic_active;
static volatile bool s_mic_idle = true;
static volatile uint32_t s_mic_ignore_until_ms;
static uint64_t s_burst_until_ms;

static void handle_key(bsp_btn_t btn, bsp_btn_ev_t ev);
static void handle_mic_drop(void);

static void play_event_sound(tetris_event_t event) {
    const char *song = NULL;
    if (event == TETRIS_EVENT_LOCKED) song = SFX_LAND;
    if (event == TETRIS_EVENT_LINE_CLEAR) song = SFX_LINE_CLEAR;
    if (event == TETRIS_EVENT_GAME_OVER) song = SFX_GAME_OVER;
    if (song && rtttl_player_play(song) != ESP_OK) {
        ESP_LOGW(TAG, "sound queue busy for event=%d", event);
    }
}

static uint64_t now_ms(void) {
    return (uint64_t)esp_timer_get_time() / 1000ULL;
}

static bool before_deadline(uint32_t time_ms, uint32_t deadline_ms) {
    return (int32_t)(time_ms - deadline_ms) < 0;
}

static uint32_t max_u32(uint32_t a, uint32_t b) {
    return a > b ? a : b;
}

static void mic_detector_task(void *arg) {
    (void)arg;
    int16_t samples[MIC_FRAME_SAMPLES];
    uint32_t noise_level = MIC_NOISE_FLOOR;
    uint32_t calibration_frames = 0;
    uint32_t quiet_frames = 0;
    uint32_t cooldown_until_ms = 0;
    uint32_t speaker_guard_until_ms = 0;
    bool armed = true;
    bool was_active = false;

    for (;;) {
        if (!s_mic_active) {
            s_mic_idle = true;
            was_active = false;
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (!was_active) {
            noise_level = MIC_NOISE_FLOOR;
            calibration_frames = 0;
            quiet_frames = 0;
            cooldown_until_ms = 0;
            speaker_guard_until_ms = 0;
            armed = true;
            was_active = true;
        }

        s_mic_idle = false;
        if (bsp_audio_read(samples, sizeof(samples)) != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (!s_mic_active) continue;

        uint64_t absolute_sum = 0;
        uint32_t peak = 0;
        for (size_t i = 0; i < MIC_FRAME_SAMPLES; i++) {
            int32_t sample = samples[i];
            uint32_t absolute = (uint32_t)(sample < 0 ? -sample : sample);
            absolute_sum += absolute;
            if (absolute > peak) peak = absolute;
        }
        uint32_t level = (uint32_t)(absolute_sum / MIC_FRAME_SAMPLES);
        uint32_t time_ms = (uint32_t)now_ms();

        if (rtttl_player_is_playing()) {
            speaker_guard_until_ms = time_ms + MIC_SPEAKER_GUARD_MS;
            continue;
        }
        if (before_deadline(time_ms, speaker_guard_until_ms) ||
            before_deadline(time_ms, s_mic_ignore_until_ms)) {
            continue;
        }

        if (calibration_frames < MIC_CALIBRATION_FRAMES) {
            noise_level = (noise_level * calibration_frames + level) /
                          (calibration_frames + 1U);
            calibration_frames++;
            if (calibration_frames == MIC_CALIBRATION_FRAMES) {
                ESP_LOGI(TAG, "mic ready: noise=%lu", (unsigned long)noise_level);
            }
            continue;
        }

        uint32_t trigger_level = max_u32(noise_level * 4U, MIC_MIN_AVG_LEVEL);
        uint32_t trigger_peak = max_u32(noise_level * 9U, MIC_MIN_PEAK_LEVEL);
        uint32_t quiet_level = max_u32(noise_level * 2U, MIC_MIN_AVG_LEVEL / 2U);

        if (level < noise_level * 2U) {
            noise_level = (noise_level * 31U + level) / 32U;
            if (noise_level < MIC_NOISE_FLOOR) noise_level = MIC_NOISE_FLOOR;
        }

        if (armed && !before_deadline(time_ms, cooldown_until_ms) &&
            level >= trigger_level && peak >= trigger_peak) {
            tetris_input_t input = { .mic_drop = true };
            if (s_input_queue && xQueueSend(s_input_queue, &input, 0) == pdTRUE) {
                ESP_LOGI(TAG, "mic drop: level=%lu peak=%lu noise=%lu",
                         (unsigned long)level, (unsigned long)peak,
                         (unsigned long)noise_level);
                ESP_LOGD(TAG, "mic task stack remaining=%lu",
                         (unsigned long)uxTaskGetStackHighWaterMark(NULL));
            }
            armed = false;
            quiet_frames = 0;
            cooldown_until_ms = time_ms + MIC_COOLDOWN_MS;
        } else if (!armed) {
            quiet_frames = level <= quiet_level ? quiet_frames + 1U : 0U;
            if (quiet_frames >= MIC_REARM_FRAMES &&
                !before_deadline(time_ms, cooldown_until_ms)) {
                armed = true;
                quiet_frames = 0;
            }
        }
    }
}

static void mic_detector_start(void) {
    if (!s_mic_task &&
        xTaskCreate(mic_detector_task, "tetris_mic", MIC_TASK_STACK_BYTES, NULL, 4,
                    &s_mic_task) != pdPASS) {
        s_mic_task = NULL;
        ESP_LOGE(TAG, "microphone detector allocation failed");
        return;
    }
    s_mic_ignore_until_ms = (uint32_t)now_ms() + MIC_SPEAKER_GUARD_MS;
    s_mic_active = true;
}

static void mic_detector_stop(void) {
    s_mic_active = false;
    for (int i = 0; i < 50 && !s_mic_idle; i++) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (!s_mic_idle) ESP_LOGW(TAG, "microphone detector stop timed out");
}

static lv_obj_t *label_create(lv_obj_t *parent, const lv_font_t *font,
                              uint32_t color, int x, int y) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_pos(label, x, y);
    return label;
}

static lv_obj_t *panel_create(lv_obj_t *parent, int x, int y, int w, int h) {
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, w, h);
    lv_obj_set_style_bg_color(panel, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_border_width(panel, 2, 0);
    lv_obj_set_style_radius(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

static void set_canvas_palette(lv_obj_t *canvas) {
    for (uint8_t i = 0; i < sizeof(PIECE_COLORS) / sizeof(PIECE_COLORS[0]); i++) {
        lv_canvas_set_palette(canvas, i,
                              lv_color_to_32(lv_color_hex(PIECE_COLORS[i]), LV_OPA_COVER));
    }
}

static void canvas_px(lv_obj_t *canvas, int x, int y, uint8_t color) {
    lv_draw_buf_t *draw_buf = lv_canvas_get_draw_buf(canvas);
    uint8_t *data = lv_draw_buf_goto_xy(draw_buf, x, y);
    if (!data) return;
    uint8_t shift = (uint8_t)(4 - 4 * (x & 1));
    *data = (uint8_t)((*data & ~(0x0FU << shift)) | ((color & 0x0FU) << shift));
}

static void render_burst(void) {
    static const char *const sprite[] = {
        "0001000000100000",
        "0000100001000000",
        "0010010010000100",
        "0000011110001000",
        "1000123321000001",
        "0101234432100010",
        "0012344443210100",
        "0001234432101000",
        "1112334433211111",
        "0001234432101000",
        "0012344443210100",
        "0101234432100010",
        "1000123321000001",
        "0000011110001000",
        "0010010010000100",
        "0100100001001000",
    };

    lv_canvas_fill_bg(s_burst_canvas, lv_color_hex(0), LV_OPA_TRANSP);
    for (int sy = 0; sy < 16; sy++) {
        for (int sx = 0; sx < 16; sx++) {
            uint8_t color = (uint8_t)(sprite[sy][sx] - '0');
            if (!color) continue;
            for (int py = 0; py < BURST_PIXEL; py++) {
                for (int px = 0; px < BURST_PIXEL; px++) {
                    canvas_px(s_burst_canvas, sx * BURST_PIXEL + px,
                              sy * BURST_PIXEL + py, color);
                }
            }
        }
    }
    lv_obj_invalidate(s_burst_canvas);
}

static void show_voice_burst(void) {
    if (!s_burst_canvas) return;
    s_burst_until_ms = now_ms() + BURST_VISIBLE_MS;
    lv_obj_remove_flag(s_burst_canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_burst_canvas);
}

static void canvas_cell(lv_obj_t *canvas, int cell_x, int cell_y, uint8_t color) {
    int start_x = cell_x * CELL_PITCH;
    int start_y = cell_y * CELL_PITCH;

    for (int y = 0; y < CELL_SIZE; y++) {
        for (int x = 0; x < CELL_SIZE; x++) {
            uint8_t pixel = color;
            if (color == 8) {
                /* Ghost piece is an outline, so it cannot look like a locked block. */
                if (x > 0 && x < CELL_SIZE - 1 && y > 0 && y < CELL_SIZE - 1) continue;
            } else if (x == 0 || y == 0) {
                pixel = 9;
            } else if (x == CELL_SIZE - 1 || y == CELL_SIZE - 1) {
                pixel = 10;
            }
            canvas_px(canvas, start_x + x, start_y + y, pixel);
        }
    }
}

static void canvas_clear_cell(lv_obj_t *canvas, int cell_x, int cell_y) {
    int start_x = cell_x * CELL_PITCH;
    int start_y = cell_y * CELL_PITCH;
    for (int y = 0; y < CELL_PITCH; y++) {
        for (int x = 0; x < CELL_PITCH; x++) {
            canvas_px(canvas, start_x + x, start_y + y, 0);
        }
    }
}

static void render_piece(lv_obj_t *canvas, tetris_piece_t piece, uint8_t rotation,
                         int origin_x, int origin_y, uint8_t color,
                         int max_width, int max_height) {
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (!tetris_piece_cell(piece, rotation, px, py)) continue;
            int x = origin_x + px;
            int y = origin_y + py;
            if (x >= 0 && x < max_width && y >= 0 && y < max_height) {
                canvas_cell(canvas, x, y, color);
            }
        }
    }
}

static void visual_capture(void) {
    s_visual.piece = s_model.piece;
    s_visual.rotation = s_model.rotation;
    s_visual.x = s_model.x;
    s_visual.y = s_model.y;
    s_visual.ghost_y = (int8_t)tetris_model_ghost_y(&s_model);
    s_visual.valid = s_model.state != TETRIS_GAME_OVER;
}

static void restore_piece_cells(const tetris_visual_t *visual, int origin_y) {
    if (!visual->valid) return;
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (!tetris_piece_cell(visual->piece, visual->rotation, px, py)) continue;
            int x = visual->x + px;
            int y = origin_y + py;
            if (x < 0 || x >= TETRIS_BOARD_WIDTH || y < 0 || y >= TETRIS_BOARD_HEIGHT) continue;
            canvas_clear_cell(s_board_canvas, x, y);
            if (s_model.board[y][x]) canvas_cell(s_board_canvas, x, y, s_model.board[y][x]);
        }
    }
}

static void invalidate_piece(const tetris_visual_t *visual, int origin_y) {
    if (!visual->valid) return;
    int min_x = TETRIS_BOARD_WIDTH;
    int min_y = TETRIS_BOARD_HEIGHT;
    int max_x = -1;
    int max_y = -1;
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (!tetris_piece_cell(visual->piece, visual->rotation, px, py)) continue;
            int x = visual->x + px;
            int y = origin_y + py;
            if (x < 0 || x >= TETRIS_BOARD_WIDTH || y < 0 || y >= TETRIS_BOARD_HEIGHT) continue;
            if (x < min_x) min_x = x;
            if (x > max_x) max_x = x;
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;
        }
    }
    if (max_x < min_x || max_y < min_y) return;
    lv_area_t base;
    lv_obj_get_coords(s_board_canvas, &base);
    lv_area_t area = {
        .x1 = base.x1 + min_x * CELL_PITCH,
        .y1 = base.y1 + min_y * CELL_PITCH,
        .x2 = base.x1 + (max_x + 1) * CELL_PITCH - 1,
        .y2 = base.y1 + (max_y + 1) * CELL_PITCH - 1,
    };
    lv_obj_invalidate_area(s_board_canvas, &area);
}

static void render_board_full(void) {
    lv_canvas_fill_bg(s_board_canvas, lv_color_hex(0), LV_OPA_COVER);
    for (int y = 0; y < TETRIS_BOARD_HEIGHT; y++) {
        for (int x = 0; x < TETRIS_BOARD_WIDTH; x++) {
            if (s_model.board[y][x]) canvas_cell(s_board_canvas, x, y, s_model.board[y][x]);
        }
    }

    if (s_model.state != TETRIS_GAME_OVER) {
        int ghost_y = tetris_model_ghost_y(&s_model);
        render_piece(s_board_canvas, s_model.piece, s_model.rotation,
                     s_model.x, ghost_y, 8, TETRIS_BOARD_WIDTH, TETRIS_BOARD_HEIGHT);
        render_piece(s_board_canvas, s_model.piece, s_model.rotation,
                     s_model.x, s_model.y, (uint8_t)s_model.piece,
                     TETRIS_BOARD_WIDTH, TETRIS_BOARD_HEIGHT);
    }
    visual_capture();
    lv_obj_invalidate(s_board_canvas);
}

static void render_board_incremental(void) {
    tetris_visual_t old = s_visual;
    restore_piece_cells(&old, old.y);
    restore_piece_cells(&old, old.ghost_y);

    visual_capture();
    if (s_visual.valid) {
        render_piece(s_board_canvas, s_visual.piece, s_visual.rotation,
                     s_visual.x, s_visual.ghost_y, 8,
                     TETRIS_BOARD_WIDTH, TETRIS_BOARD_HEIGHT);
        render_piece(s_board_canvas, s_visual.piece, s_visual.rotation,
                     s_visual.x, s_visual.y, (uint8_t)s_visual.piece,
                     TETRIS_BOARD_WIDTH, TETRIS_BOARD_HEIGHT);
    }

    invalidate_piece(&old, old.y);
    invalidate_piece(&old, old.ghost_y);
    invalidate_piece(&s_visual, s_visual.y);
    invalidate_piece(&s_visual, s_visual.ghost_y);
}

static void render_next(void) {
    lv_canvas_fill_bg(s_next_canvas, lv_color_hex(0), LV_OPA_COVER);
    render_piece(s_next_canvas, s_model.next_piece, 0, 0, 0,
                 (uint8_t)s_model.next_piece, 4, 4);
    lv_obj_invalidate(s_next_canvas);
}

static void refresh_status(void) {
    if (s_model.state == TETRIS_PAUSED) {
        lv_label_set_text(s_status_label, "PAUSED\n\nHOLD UP");
        lv_obj_remove_flag(s_status_label, LV_OBJ_FLAG_HIDDEN);
    } else if (s_model.state == TETRIS_GAME_OVER) {
        lv_label_set_text(s_status_label, "GAME OVER\n\nOK RESTART");
        lv_obj_remove_flag(s_status_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_status_label, LV_OBJ_FLAG_HIDDEN);
    }
}

static void refresh_ui(void) {
    if (!s_scr) return;
    render_board_full();
    render_next();
    lv_label_set_text_fmt(s_stats_label,
                          "SCORE\n%06lu\n\nLINES  %u\nLEVEL  %u",
                          (unsigned long)s_model.score,
                          (unsigned)s_model.lines,
                          (unsigned)s_model.level);
    refresh_status();
}

static bool primary_key_event(bsp_btn_t btn, bsp_btn_ev_t ev, uint64_t time_ms) {
    if ((unsigned)btn >= 3U) return false;
    if (ev == BSP_BTN_PRESS) {
        s_press_seen[btn] = true;
        s_last_press_ms[btn] = time_ms;
        return true;
    }
    /* Some button component versions only deliver CLICK reliably. Ignore the
       CLICK paired with a recent PRESS so one physical press moves one cell. */
    return ev == BSP_BTN_CLICK &&
           (!s_press_seen[btn] || time_ms - s_last_press_ms[btn] > 1500U);
}

static void show_input(const char *text, bool accepted) {
    if (!s_input_label) return;
    lv_label_set_text_fmt(s_input_label, "%s%s", text, accepted ? "" : " BLOCK");
    lv_obj_set_style_text_color(s_input_label,
                                lv_color_hex(accepted ? COLOR_ACCENT : 0xFF6670), 0);
}

static void timer_cb(lv_timer_t *timer) {
    (void)timer;
    tetris_input_t input;
    while (s_input_queue && xQueueReceive(s_input_queue, &input, 0) == pdTRUE) {
        if (input.mic_drop) handle_mic_drop();
        else handle_key(input.btn, input.ev);
    }
    uint64_t time_ms = now_ms();
    if (s_burst_canvas && s_burst_until_ms && time_ms >= s_burst_until_ms) {
        lv_obj_add_flag(s_burst_canvas, LV_OBJ_FLAG_HIDDEN);
        s_burst_until_ms = 0;
    }
    if (s_model.state != TETRIS_PLAYING) return;
    uint32_t interval = tetris_model_drop_interval_ms(&s_model);
    if (time_ms - s_last_drop_ms < interval) return;
    s_last_drop_ms = time_ms;
    tetris_event_t event = tetris_model_step(&s_model);
    play_event_sound(event);
    if (event == TETRIS_EVENT_MOVED) render_board_incremental();
    else if (event != TETRIS_EVENT_NONE) {
        s_up_press_shifted = false;
        refresh_ui();
    }
}

void demo_tetris_enter(void) {
    s_input_queue = xQueueCreate(8, sizeof(tetris_input_t));
    if (!s_input_queue) ESP_LOGE(TAG, "input queue allocation failed");
    tetris_model_init(&s_model, (uint32_t)esp_timer_get_time());
    s_last_drop_ms = now_ms();
    for (int i = 0; i < 3; i++) {
        s_last_press_ms[i] = 0;
        s_press_seen[i] = false;
    }
    s_visual.valid = false;
    s_up_press_shifted = false;
    s_burst_until_ms = 0;
    if (bsp_audio_set_format(MIC_SAMPLE_RATE, 16, 1) == ESP_OK &&
        rtttl_player_start() == ESP_OK) {
        mic_detector_start();
        rtttl_player_play(SFX_START);
    } else {
        ESP_LOGW(TAG, "game audio or microphone unavailable");
    }

    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_scr, 0, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = label_create(s_scr, &lv_font_montserrat_20, COLOR_ACCENT, 8, 10);
    lv_label_set_text(title, "TETRIS");
    s_input_label = label_create(s_scr, &lv_font_montserrat_14, COLOR_DIM, 89, 16);
    lv_obj_set_width(s_input_label, 145);
    lv_obj_set_style_text_align(s_input_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(s_input_label, "READY");

    panel_create(s_scr, 5, 53, 126, 246);
    LV_DRAW_BUF_INIT_STATIC(board_buf);
    s_board_canvas = lv_canvas_create(s_scr);
    lv_canvas_set_draw_buf(s_board_canvas, &board_buf);
    set_canvas_palette(s_board_canvas);
    lv_obj_set_pos(s_board_canvas, 8, 56);

    lv_obj_t *next_title = label_create(s_scr, &lv_font_montserrat_14, COLOR_DIM, 153, 42);
    lv_label_set_text(next_title, "NEXT");
    panel_create(s_scr, 147, 59, 60, 60);
    LV_DRAW_BUF_INIT_STATIC(next_buf);
    s_next_canvas = lv_canvas_create(s_scr);
    lv_canvas_set_draw_buf(s_next_canvas, &next_buf);
    set_canvas_palette(s_next_canvas);
    lv_obj_set_pos(s_next_canvas, 153, 65);

    s_stats_label = label_create(s_scr, &lv_font_montserrat_14, COLOR_TEXT, 145, 132);
    lv_obj_set_style_text_line_space(s_stats_label, 1, 0);

    lv_obj_t *controls = label_create(s_scr, &lv_font_montserrat_14, COLOR_DIM, 143, 227);
    lv_obj_set_style_text_line_space(controls, -1, 0);
    lv_label_set_text(controls, "UP  LEFT\nDN  RIGHT\nOK  ROT\nVOICE  DROP\nHOLD UP PAUSE");

    s_status_label = label_create(s_scr, &lv_font_montserrat_14, COLOR_TEXT, 18, 134);
    lv_obj_set_size(s_status_label, 100, 68);
    lv_obj_set_style_text_align(s_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(s_status_label, lv_color_hex(0x0B111C), 0);
    lv_obj_set_style_bg_opa(s_status_label, LV_OPA_90, 0);
    lv_obj_set_style_border_color(s_status_label, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_border_width(s_status_label, 2, 0);
    lv_obj_set_style_pad_top(s_status_label, 8, 0);

    LV_DRAW_BUF_INIT_STATIC(burst_buf);
    s_burst_canvas = lv_canvas_create(s_scr);
    lv_canvas_set_draw_buf(s_burst_canvas, &burst_buf);
    lv_canvas_set_palette(s_burst_canvas, 0,
                          lv_color_to_32(lv_color_hex(0), LV_OPA_TRANSP));
    lv_canvas_set_palette(s_burst_canvas, 1,
                          lv_color_to_32(lv_color_hex(0xFF3B30), LV_OPA_COVER));
    lv_canvas_set_palette(s_burst_canvas, 2,
                          lv_color_to_32(lv_color_hex(0xFF8A24), LV_OPA_COVER));
    lv_canvas_set_palette(s_burst_canvas, 3,
                          lv_color_to_32(lv_color_hex(0xFFD84A), LV_OPA_COVER));
    lv_canvas_set_palette(s_burst_canvas, 4,
                          lv_color_to_32(lv_color_hex(0xFFF8D8), LV_OPA_COVER));
    lv_obj_set_pos(s_burst_canvas, 36, 144);
    render_burst();
    lv_obj_add_flag(s_burst_canvas, LV_OBJ_FLAG_HIDDEN);

    refresh_ui();
    s_timer = lv_timer_create(timer_cb, 20, NULL);
    lv_screen_load(s_scr);

    lv_mem_monitor_t memory;
    lv_mem_monitor(&memory);
    ESP_LOGI(TAG, "ready: %u bytes LVGL free, %u%% fragmented",
             (unsigned)memory.free_size, memory.frag_pct);
}

void demo_tetris_exit(void) {
    mic_detector_stop();
    rtttl_player_stop();
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
    }
    if (s_input_queue) {
        vQueueDelete(s_input_queue);
        s_input_queue = NULL;
    }
    s_board_canvas = s_next_canvas = s_stats_label = s_status_label = s_input_label = NULL;
    s_burst_canvas = NULL;
    s_burst_until_ms = 0;
}

static void handle_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    bool changed = false;
    bool board_only = false;
    bool full_refresh = false;
    uint64_t time_ms = now_ms();
    bool primary = primary_key_event(btn, ev, time_ms);
    if (primary && s_model.state != TETRIS_GAME_OVER) {
        rtttl_player_play(SFX_KEY);
    }

    if (s_model.state == TETRIS_GAME_OVER) {
        if (btn == BSP_BTN_OK && primary) {
            tetris_model_reset(&s_model);
            s_last_drop_ms = time_ms;
            changed = true;
            full_refresh = true;
            show_input("RESTART", true);
            rtttl_player_play(SFX_START);
        }
    } else if (ev == BSP_BTN_LONG) {
        if (btn == BSP_BTN_UP) {
            if (s_model.state == TETRIS_PLAYING && s_up_press_shifted) {
                tetris_model_move(&s_model, 1);
            }
            changed = tetris_model_toggle_pause(&s_model);
            s_last_drop_ms = time_ms;
            s_up_press_shifted = false;
            show_input(s_model.state == TETRIS_PAUSED ? "PAUSE" : "RESUME", changed);
            if (changed) refresh_status();
        }
    } else if (primary && s_model.state == TETRIS_PLAYING) {
        if (btn == BSP_BTN_UP) {
            changed = tetris_model_move(&s_model, -1);
            s_up_press_shifted = changed;
            show_input("LEFT", changed);
            board_only = changed;
        } else if (btn == BSP_BTN_DOWN) {
            changed = tetris_model_move(&s_model, 1);
            show_input("RIGHT", changed);
            board_only = changed;
        } else if (btn == BSP_BTN_OK) {
            changed = tetris_model_rotate(&s_model);
            show_input("ROTATE", changed);
            board_only = changed;
        }
    }

    if (ev == BSP_BTN_PRESS || ev == BSP_BTN_CLICK || ev == BSP_BTN_LONG) {
        ESP_LOGI(TAG, "key=%d event=%d primary=%d changed=%d state=%d",
                 btn, ev, primary, changed, s_model.state);
    }
    if (board_only) render_board_incremental();
    else if (full_refresh) refresh_ui();
}

static void handle_mic_drop(void) {
    if (s_model.state != TETRIS_PLAYING) return;
    tetris_event_t event = tetris_model_hard_drop(&s_model);
    play_event_sound(event);
    s_last_drop_ms = now_ms();
    s_up_press_shifted = false;
    show_input("VOICE DROP", true);
    refresh_ui();
    show_voice_burst();
}

void demo_tetris_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    if (!s_input_queue) return;
    if (ev == BSP_BTN_PRESS || ev == BSP_BTN_CLICK || ev == BSP_BTN_LONG) {
        s_mic_ignore_until_ms = (uint32_t)now_ms() + MIC_BUTTON_GUARD_MS;
    }
    tetris_input_t input = { .btn = btn, .ev = ev, .mic_drop = false };
    if (xQueueSend(s_input_queue, &input, 0) != pdTRUE) {
        ESP_LOGW(TAG, "input queue full: key=%d event=%d", btn, ev);
    }
}
