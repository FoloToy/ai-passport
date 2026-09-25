// eggy_common.h — shared types for the eggy party app.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bsp_button.h"

#define EGGY_SCREEN_W   240
#define EGGY_SCREEN_H   320

// ---- Party palette (RGB565-ish hex, used via lv_color_hex) ----
#define EGGY_COLOR_BG       0x1A1033   // deep party purple
#define EGGY_COLOR_PANEL    0x2A1A4A
#define EGGY_COLOR_PANEL2   0x3A2A66
#define EGGY_COLOR_INK      0xF4F1FF
#define EGGY_COLOR_DIM      0x9A8BC0
#define EGGY_COLOR_PINK     0xFF5DA2
#define EGGY_COLOR_CYAN     0x2EC4B6
#define EGGY_COLOR_YELLOW   0xFFE45C   // eggy body
#define EGGY_COLOR_GREEN    0x46E46F
#define EGGY_COLOR_RED      0xFF4A57
#define EGGY_COLOR_GROUND   0x3A2A5A

// Identifiers for the four top-level screens.
typedef enum {
    EGGY_SCREEN_HUB = 0,
    EGGY_SCREEN_RUN,
    EGGY_SCREEN_TAP,
    EGGY_SCREEN_SPIN,
} eggy_screen_id_t;

// A queued button event. Button callbacks enqueue this; the dispatch task
// drains it under the LVGL lock and routes to the active screen.
typedef struct {
    bsp_btn_t btn;
    bsp_btn_ev_t ev;
} eggy_input_t;

// Lifecycle of a top-level screen. enter() builds its LVGL screen and makes it
// active; exit() tears down timers/queues and deletes the screen; key() routes
// a decoded input. main owns the active screen pointer.
typedef struct {
    eggy_screen_id_t id;
    void (*enter)(void);
    void (*exit)(void);
    void (*key)(const eggy_input_t *in);
} eggy_screen_t;

// Idle deep-sleep timeouts (ms). Reset on any input.
#define EGGY_IDLE_SLEEP_HUB_MS    60000
#define EGGY_IDLE_SLEEP_GAME_MS   120000

// Notify the idle supervisor that input arrived (resets the idle timer).
void eggy_idle_notify_activity(void);

// Start the idle supervisor (call once from app_main after BSP init).
void eggy_sleep_supervisor_start(void);

// Switch the idle timeout between the longer hub value and the shorter in-game
// value; also resets the idle timer.
void eggy_sleep_set_in_game(bool in_game);

// True if this boot is a wake from the eggy-party deep sleep (vs cold start).
bool eggy_slept_before(void);

// Switch to a top-level screen: tears down the current screen (if any) and
// enters the named one. The hub is the central return target for OK-long.
void eggy_switch_to(eggy_screen_id_t id);
