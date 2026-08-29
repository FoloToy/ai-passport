#include "funbox_model.h"

#include <string.h>

static uint8_t option_count(uint8_t app)
{
    if (app == 1) return 3; /* Random name: cute, classic, futuristic. */
    if (app == 4) return 3; /* Dice count: one, two, or three dice. */
    return 1;
}
void funbox_model_init(funbox_model_t *model, uint32_t seed)
{
    memset(model, 0, sizeof(*model));
    model->page = FUNBOX_PAGE_HOME;
    model->volume_step = 3;
    model->brightness_step = 4;
    model->bgm_enabled = true;
    model->rng = seed ? seed : 0x51A7B00BU;
}

uint32_t funbox_model_random(funbox_model_t *model, uint32_t limit)
{
    uint32_t x = model->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    model->rng = x ? x : 0x51A7B00BU;
    return limit ? model->rng % limit : 0;
}

static bool adjust_option(funbox_model_t *model, int direction)
{
    uint8_t count = option_count(model->current_app);
    if (count <= 1) return false;
    model->option = (uint8_t)((model->option + count + direction) % count);
    return true;
}

bool funbox_model_apply(funbox_model_t *model, funbox_action_t action)
{
    if (action == FUNBOX_ACTION_LOVE) {
        model->page = FUNBOX_PAGE_LOVE;
        model->current_app = 10;
        model->generation++;
        return true;
    }
    if (action == FUNBOX_ACTION_BACK) {
        if (model->page == FUNBOX_PAGE_HOME) return false;
        model->page = FUNBOX_PAGE_HOME;
        model->selected = model->current_app;
        return true;
    }

    if (model->page == FUNBOX_PAGE_HOME) {
        if (action == FUNBOX_ACTION_PREV) {
            model->selected = (uint8_t)((model->selected + FUNBOX_MENU_COUNT - 1) %
                                        FUNBOX_MENU_COUNT);
            return true;
        }
        if (action == FUNBOX_ACTION_NEXT) {
            model->selected = (uint8_t)((model->selected + 1) % FUNBOX_MENU_COUNT);
            return true;
        }
        if (action == FUNBOX_ACTION_CONFIRM) {
            model->option = 0;
            if (model->selected == FUNBOX_PLAY_COUNT) {
                model->page = FUNBOX_PAGE_SETTINGS;
            } else {
                model->current_app = model->selected;
                model->page = model->current_app == 10 ? FUNBOX_PAGE_LOVE : FUNBOX_PAGE_APP;
                model->generation++;
            }
            return true;
        }
        return false;
    }

    if (model->page == FUNBOX_PAGE_SETTINGS) {
        if (action == FUNBOX_ACTION_PREV) {
            model->setting_row = (uint8_t)((model->setting_row + 2) % 3);
            return true;
        }
        if (action == FUNBOX_ACTION_NEXT) {
            model->setting_row = (uint8_t)((model->setting_row + 1) % 3);
            return true;
        }
        if (action != FUNBOX_ACTION_CONFIRM) return false;
        if (model->setting_row == 0) model->volume_step = (uint8_t)((model->volume_step + 1) % 5);
        if (model->setting_row == 1) model->bgm_enabled = !model->bgm_enabled;
        if (model->setting_row == 2) {
            model->brightness_step = (uint8_t)((model->brightness_step + 1) % 5);
        }
        return true;
    }

    if (model->page == FUNBOX_PAGE_LOVE) {
        if (action == FUNBOX_ACTION_CONFIRM) {
            model->generation++;
            return true;
        }
        return false;
    }

    if (model->page == FUNBOX_PAGE_APP) {
        if (action == FUNBOX_ACTION_PREV) return adjust_option(model, -1);
        if (action == FUNBOX_ACTION_NEXT) return adjust_option(model, 1);
        if (action == FUNBOX_ACTION_CONFIRM) {
            model->generation++;
            return true;
        }
    }
    return false;
}

uint8_t funbox_model_volume(const funbox_model_t *model)
{
    static const uint8_t levels[] = { 0, 25, 50, 75, 100 };
    return levels[model->volume_step < 5 ? model->volume_step : 4];
}

uint8_t funbox_model_brightness(const funbox_model_t *model)
{
    static const uint8_t levels[] = { 20, 40, 60, 80, 100 };
    return levels[model->brightness_step < 5 ? model->brightness_step : 4];
}
