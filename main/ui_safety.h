#pragma once

#include <stdbool.h>

#include "safety_profile.h"

#define UI_SAFETY_PAGE_COUNT 5

void ui_safety_show_profile(const safety_profile_t *profile, int page,
                            bool has_wechat_qr, int battery_percent);
void ui_safety_show_setup(const char *ssid, const char *password,
                          bool first_setup, int battery_percent);
void ui_safety_show_reset_confirm(int battery_percent);
void ui_safety_show_saved(int battery_percent);
void ui_safety_show_qr_loading(int battery_percent);
void ui_safety_show_qr_error(int battery_percent);
