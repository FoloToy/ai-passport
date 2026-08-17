#pragma once

#include <stdbool.h>

#include "bsp_button.h"
#include "tamagezi_model.h"

bool tmz_app_start(const tmz_model_t *initial_model, bool battery_available);
void tmz_app_button(bsp_btn_t button, bsp_btn_ev_t event);
