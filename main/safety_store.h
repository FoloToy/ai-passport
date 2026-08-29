#pragma once

#include <stdbool.h>

#include "safety_profile.h"

bool safety_store_init(void);
bool safety_store_load(safety_profile_t *profile);
bool safety_store_save(safety_profile_t *profile);
