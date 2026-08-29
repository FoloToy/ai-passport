#pragma once

#include <stdbool.h>

#include "safety_profile.h"

bool safety_portal_start(safety_profile_t *profile, bool first_setup);
void safety_portal_stop(void);
bool safety_portal_is_running(void);
bool safety_portal_take_saved(void);
const char *safety_portal_ssid(void);
const char *safety_portal_password(void);
