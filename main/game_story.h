#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "game_i18n.h"
#include "game_model.h"

game_text_id_t game_story_feedback_text(game_feedback_t feedback);
game_text_id_t game_story_hint_text(uint8_t hint_level);
game_text_id_t game_story_ending_text(game_ending_t ending);
bool game_story_ending_congratulates(game_ending_t ending);
