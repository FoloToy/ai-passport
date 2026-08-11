#include "buddy_character.h"

static const char sleep_rest[] =
    "  .---.  \n"
    " ( -_- ) \n"
    " /|   |\\ \n"
    "  /   \\  ";
static const char sleep_dream[] =
    "  .---. z\n"
    " ( -_- ) \n"
    " /|   |\\ \n"
    "  /   \\  ";
static const char idle[] =
    "  .---.  \n"
    " ( o.o ) \n"
    " /|   |\\ \n"
    "  /   \\  ";
static const char busy_a[] =
    "  .---.  \n"
    " ( >.< ) \n"
    " /|...|\\ \n"
    "  /   \\  ";
static const char busy_b[] =
    "  .---.  \n"
    " ( >.< ) \n"
    " /|:::|\\ \n"
    "  /   \\  ";
static const char attention_a[] =
    "  .---. !\n"
    " ( O.O ) \n"
    " /|   |\\ \n"
    "  /   \\  ";
static const char attention_b[] =
    " !.---.  \n"
    " ( O.O ) \n"
    " /|   |\\ \n"
    "  /   \\  ";
static const char dizzy_a[] =
    "  .---.  \n"
    " ( x.x ) \n"
    " /|   |\\ \n"
    "  /   \\  ";
static const char dizzy_b[] =
    "  .---.  \n"
    " ( +.+ ) \n"
    " /|   |\\ \n"
    "  /   \\  ";
static const char heart_a[] =
    "  .---.  \n"
    " ( ^.^ ) \n"
    " /|<3 |\\ \n"
    "  /   \\  ";
static const char heart_b[] =
    "  .---.  \n"
    " ( ^.^ ) \n"
    " /| <3|\\ \n"
    "  /   \\  ";
static const char celebrate_a[] =
    " *.---.* \n"
    " ( ^o^ ) \n"
    "\\|   |/ \n"
    "  /   \\  ";
static const char celebrate_b[] =
    "  .---.  \n"
    "*( ^o^ )*\n"
    "\\|   |/ \n"
    "  /   \\  ";
static const char pairing_a[] =
    "  .---. ?\n"
    " ( @.@ ) \n"
    " /|   |\\ \n"
    "  /   \\  ";
static const char pairing_b[] =
    " ?.---.  \n"
    " ( @.@ ) \n"
    " /|   |\\ \n"
    "  /   \\  ";
static const char confirmation_a[] =
    "  .---.  \n"
    " ( ?.? ) \n"
    " /|   |\\ \n"
    "  /   \\  ";
static const char confirmation_b[] =
    "  .---.  \n"
    " ( !.! ) \n"
    " /|   |\\ \n"
    "  /   \\  ";

static bool buddy_character_is_second_phase(uint64_t elapsed_ms, uint64_t phase_ms)
{
    return (elapsed_ms % BUDDY_CHARACTER_ANIMATION_PERIOD_MS) / phase_ms % 2U != 0U;
}

const char *buddy_character_frame(buddy_character_t state, uint64_t elapsed_ms)
{
    switch (state) {
    case BUDDY_CHARACTER_SLEEP:
        return buddy_character_is_second_phase(elapsed_ms, 1000U) ? sleep_dream : sleep_rest;
    case BUDDY_CHARACTER_IDLE:
        return idle;
    case BUDDY_CHARACTER_BUSY:
        return buddy_character_is_second_phase(elapsed_ms, 500U) ? busy_b : busy_a;
    case BUDDY_CHARACTER_ATTENTION:
        return buddy_character_is_second_phase(elapsed_ms, 500U) ? attention_b : attention_a;
    case BUDDY_CHARACTER_DIZZY:
        return buddy_character_is_second_phase(elapsed_ms, 500U) ? dizzy_b : dizzy_a;
    case BUDDY_CHARACTER_HEART:
        return buddy_character_is_second_phase(elapsed_ms, 500U) ? heart_b : heart_a;
    case BUDDY_CHARACTER_CELEBRATE:
        return buddy_character_is_second_phase(elapsed_ms, 500U) ? celebrate_b : celebrate_a;
    case BUDDY_CHARACTER_PAIRING:
        return buddy_character_is_second_phase(elapsed_ms, 500U) ? pairing_b : pairing_a;
    case BUDDY_CHARACTER_CONFIRMATION:
        return buddy_character_is_second_phase(elapsed_ms, 500U) ? confirmation_b : confirmation_a;
    default:
        return idle;
    }
}
