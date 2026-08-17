#pragma once

#include "lvgl.h"
#include "tamagezi_model.h"

lv_obj_t *tmz_pet_view_create(lv_obj_t *parent, const tmz_model_t *model,
                              int x, int y, int size);
