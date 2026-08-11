#pragma once

#include <stdint.h>

#include "buddy_types.h"

/* All character sequences loop at this shared, deterministic interval. */
#define BUDDY_CHARACTER_ANIMATION_PERIOD_MS 2000U

/* Returns an original, flash-resident ASCII frame for the requested state. */
const char *buddy_character_frame(buddy_character_t state, uint64_t elapsed_ms);
