// mini_world.c — view/controller for the 2D tile sandbox.
#include "mini_common.h"
#include "mini_world_model.h"
#include "mini_ui.h"
#include "mini_sfx.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#define TILE_PX     16
#define VIEW_W      15          // tiles visible horizontally (240/16)
#define VIEW_H      16          // tiles visible vertically (256/16)
#define CANVAS_W    (VIEW_W * TILE_PX)
#define CANVAS_H    (VIEW_H * TILE_PX)
#define CANVAS_Y    60          // top HUD strip is 0..CANVAS_Y
#define TICK_MS     30
#define WALK_BURST_MS 450       // how long one UP/DOWN press walks
#define BATTERY_REFRESH_TICKS 60

LV_DRAW_BUF_DEFINE_STATIC(world_buf, CANVAS_W, CANVAS_H, LV_COLOR_FORMAT_I4);

static mini_world_model_t s_model;
static lv_obj_t *s_scr;
static lv_obj_t *s_canvas;
static lv_obj_t *s_player;
static lv_obj_t *s_battery;
static lv_obj_t *s_digs;
static lv_obj_t *s_hint;
static lv_timer_t *s_timer;
static uint64_t s_last_us;

// Held-walk state (tap-to-walk bursts; bsp_button has no release event).
static int8_t  s_walk_dir;       // -1 left, +1 right, 0 none
static uint64_t s_walk_until;
static bool s_jump_req;
static bool s_break_req;
static bool s_place_req;

static int s_cam_x, s_cam_y;     // last rendered camera (tile units)
static int s_battery_tick;

static uint64_t now_us(void) { return (uint64_t)esp_timer_get_time(); }

// ---- I4 canvas pixel write (ported from origin/demo/tetris-game) ----
static void canvas_px(int x, int y, uint8_t idx) {
    lv_draw_buf_t *db = lv_canvas_get_draw_buf(s_canvas);
    uint8_t *data = lv_draw_buf_goto_xy(db, x, y);
    if (!data) return;
    uint8_t shift = (uint8_t)(4 - 4 * (x & 1));
    *data = (uint8_t)((*data & ~(0x0FU << shift)) | ((idx & 0x0FU) << shift));
}

static void set_palette(void) {
    // Palette index == tile type constant (AIR=0..WOOD=4).
    static const uint32_t colors[] = {
        MINI_COLOR_SKY, MINI_COLOR_GRASS, MINI_COLOR_DIRT,
        MINI_COLOR_STONE, MINI_COLOR_WOOD,
    };
    for (uint8_t i = 0; i < sizeof(colors) / sizeof(colors[0]); i++) {
        lv_canvas_set_palette(s_canvas, i,
            lv_color_to_32(lv_color_hex(colors[i]), LV_OPA_COVER));
    }
}

static void draw_tile(int vx, int vy, uint8_t idx) {
    int sx = vx * TILE_PX, sy = vy * TILE_PX;
    for (int y = 0; y < TILE_PX; y++) {
        for (int x = 0; x < TILE_PX; x++) {
            canvas_px(sx + x, sy + y, idx);
        }
    }
}

static void redraw_world(void) {
    for (int vy = 0; vy < VIEW_H; vy++) {
        for (int vx = 0; vx < VIEW_W; vx++) {
            int tx = s_cam_x + vx;
            int ty = s_cam_y + vy;
            uint8_t idx = MINI_AIR;
            if (tx >= 0 && tx < MINI_WORLD_W && ty >= 0 && ty < MINI_WORLD_H) {
                idx = s_model.grid[ty][tx];
            }
            draw_tile(vx, vy, idx);
        }
    }
    lv_obj_invalidate(s_canvas);
}

static void update_camera(void) {
    int cx = (int)(s_model.px + MINI_PW * 0.5f) - VIEW_W / 2;
    int cy = (int)(s_model.py + MINI_PH * 0.5f) - VIEW_H / 2;
    if (cx < 0) cx = 0;
    if (cx > MINI_WORLD_W - VIEW_W) cx = MINI_WORLD_W - VIEW_W;
    if (cy < 0) cy = 0;
    if (cy > MINI_WORLD_H - VIEW_H) cy = MINI_WORLD_H - VIEW_H;
    s_cam_x = cx;
    s_cam_y = cy;
}

static void move_player_obj(void) {
    int sx = (int)((s_model.px - (float)s_cam_x) * (float)TILE_PX);
    int sy = (int)((s_model.py - (float)s_cam_y) * (float)TILE_PX);
    lv_obj_set_pos(s_player, sx, sy);
}

static void tick_cb(lv_timer_t *t) {
    (void)t;
    uint64_t now = now_us();
    uint32_t dt = (uint32_t)((now - s_last_us) / 1000);
    s_last_us = now;
    if (dt == 0) dt = 1;
    if (dt > 100) dt = 100;

    bool left = (s_walk_dir < 0) && (now < s_walk_until);
    bool right = (s_walk_dir > 0) && (now < s_walk_until);
    bool jump = s_jump_req;
    s_jump_req = false;

    bool was_grounded = s_model.on_ground;
    mini_world_step(&s_model, dt, left, right, jump);
    if (jump && was_grounded && s_model.vy < 0.0f) {
        mini_sfx_play(MINI_SFX_JUMP);
    } else if (s_model.on_ground && !was_grounded) {
        mini_sfx_play(MINI_SFX_LAND);
    }

    if (s_break_req) {
        s_break_req = false;
        if (mini_world_break(&s_model)) mini_sfx_play(MINI_SFX_DIG);
    }
    if (s_place_req) {
        s_place_req = false;
        if (mini_world_place(&s_model)) mini_sfx_play(MINI_SFX_PLACE);
    }

    int prev_cx = s_cam_x, prev_cy = s_cam_y;
    update_camera();
    if (s_cam_x != prev_cx || s_cam_y != prev_cy) {
        redraw_world();   // camera scrolled
    }
    move_player_obj();

    if (++s_battery_tick >= BATTERY_REFRESH_TICKS) {
        s_battery_tick = 0;
        mini_battery_badge_refresh(s_battery);
        lv_label_set_text_fmt(s_digs, "DIGS %d", (int)s_model.digs);
    }
}

static void build_player(void) {
    // Player is a child of the canvas so it draws on top of the world and
    // moves in canvas-relative coordinates.
    s_player = lv_obj_create(s_canvas);
    lv_obj_set_size(s_player, 12, 26);
    lv_obj_set_style_bg_color(s_player, lv_color_hex(MINI_COLOR_PLAYER), 0);
    lv_obj_set_style_bg_opa(s_player, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_player, 5, 0);
    lv_obj_set_style_border_width(s_player, 0, 0);
    lv_obj_set_style_pad_all(s_player, 0, 0);
    lv_obj_clear_flag(s_player, LV_OBJ_FLAG_SCROLLABLE);
    for (int e = 0; e < 2; e++) {
        lv_obj_t *eye = lv_obj_create(s_player);
        lv_obj_set_size(eye, 3, 3);
        lv_obj_set_style_bg_color(eye, lv_color_hex(0x201830), 0);
        lv_obj_set_style_bg_opa(eye, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(eye, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(eye, 0, 0);
        lv_obj_align(eye, LV_ALIGN_TOP_MID, e == 0 ? -3 : 3, 4);
    }
}

void mini_world_enter(void) {
    LV_DRAW_BUF_INIT_STATIC(world_buf);
    mini_world_generate(&s_model, (uint32_t)now_us());

    s_scr = mini_screen_create();
    s_battery = mini_battery_badge_create(s_scr);
    s_digs = mini_label_create(s_scr, 8, 4, "DIGS 0",
                               &lv_font_montserrat_14, MINI_COLOR_YELLOW);

    s_canvas = lv_canvas_create(s_scr);
    lv_canvas_set_draw_buf(s_canvas, &world_buf);
    lv_obj_set_pos(s_canvas, 0, CANVAS_Y);
    set_palette();

    s_hint = lv_label_create(s_scr);
    lv_label_set_text(s_hint,
        "U/D: walk   OK: jump   hold OK: dig   2x OK: place");
    lv_obj_set_style_text_color(s_hint, lv_color_hex(MINI_COLOR_DIM), 0);
    lv_obj_set_style_text_font(s_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(s_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_hint, 240);
    lv_label_set_long_mode(s_hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(s_hint, 0, 22);

    build_player();

    s_cam_x = s_cam_y = -1;  // force a redraw
    update_camera();
    redraw_world();
    move_player_obj();
    mini_battery_badge_refresh(s_battery);

    s_walk_dir = 0;
    s_walk_until = 0;
    s_jump_req = s_break_req = s_place_req = false;
    s_last_us = now_us();
    s_battery_tick = 0;
    s_timer = lv_timer_create(tick_cb, TICK_MS, NULL);
    lv_timer_ready(s_timer);
}

void mini_world_exit(void) {
    if (s_timer) {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }
    if (s_scr) {
        lv_obj_delete(s_scr);
    }
    s_scr = NULL;
    s_canvas = NULL;
    s_player = NULL;
    s_walk_dir = 0;
    s_jump_req = s_break_req = s_place_req = false;
}

void mini_world_key(const mini_input_t *in) {
    // UP/DOWN: tap-to-walk bursts. OK: click=jump, long=dig, double=place.
    if (in->btn == BSP_BTN_UP) {
        if (in->ev == BSP_BTN_PRESS) {
            s_walk_dir = -1;
            s_walk_until = now_us() + (uint64_t)WALK_BURST_MS * 1000;
        } else if (in->ev == BSP_BTN_CLICK) {
            s_walk_dir = 0;
        }
    } else if (in->btn == BSP_BTN_DOWN) {
        if (in->ev == BSP_BTN_PRESS) {
            s_walk_dir = +1;
            s_walk_until = now_us() + (uint64_t)WALK_BURST_MS * 1000;
        } else if (in->ev == BSP_BTN_CLICK) {
            s_walk_dir = 0;
        }
    } else if (in->btn == BSP_BTN_OK) {
        if (in->ev == BSP_BTN_CLICK) {
            s_jump_req = true;
        } else if (in->ev == BSP_BTN_LONG) {
            s_break_req = true;
        } else if (in->ev == BSP_BTN_DOUBLE) {
            s_place_req = true;
        }
    }
}
