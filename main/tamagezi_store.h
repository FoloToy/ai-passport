#pragma once

#include <stdbool.h>

#include "tamagezi_model.h"

bool tmz_store_init(void);
bool tmz_store_load(tmz_model_t *model);
bool tmz_store_save_async(const tmz_model_t *model);
