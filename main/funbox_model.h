#pragma once

#include <stdbool.h>
#include <stdint.h>

#define FUNBOX_PLAY_COUNT 11
#define FUNBOX_MENU_COUNT (FUNBOX_PLAY_COUNT + 1)

typedef enum {
    FUNBOX_PAGE_HOME = 0,
    FUNBOX_PAGE_APP,
    FUNBOX_PAGE_SETTINGS,
    FUNBOX_PAGE_LOVE,
} funbox_page_t;

typedef enum {
    FUNBOX_ACTION_PREV = 0,
    FUNBOX_ACTION_NEXT,
    FUNBOX_ACTION_CONFIRM,
    FUNBOX_ACTION_BACK,
    FUNBOX_ACTION_LOVE,
} funbox_action_t;

typedef struct {
    funbox_page_t page;
    uint8_t selected;
    uint8_t current_app;
    uint8_t option;
    uint8_t setting_row;
    uint8_t volume_step;
    uint8_t brightness_step;
    bool bgm_enabled;
    uint32_t generation;
    uint32_t rng;
} funbox_model_t;

void funbox_model_init(funbox_model_t *model, uint32_t seed);
bool funbox_model_apply(funbox_model_t *model, funbox_action_t action);
uint32_t funbox_model_random(funbox_model_t *model, uint32_t limit);
uint8_t funbox_model_volume(const funbox_model_t *model);
uint8_t funbox_model_brightness(const funbox_model_t *model);
