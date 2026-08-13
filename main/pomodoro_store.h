#pragma once

#include <stdbool.h>

#include "pomodoro_model.h"

bool pomodoro_store_init(pomodoro_model_t *model);
void pomodoro_store_request_save(const pomodoro_model_t *model);
bool pomodoro_store_has_error(void);
