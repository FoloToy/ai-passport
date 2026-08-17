#include "tamagezi_model.h"

#include <string.h>

static const tmz_pet_def_t PETS[TMZ_PET_COUNT] = {
    { "SPRIG",  "SPROUT", TMZ_FOOD_GREENS, TMZ_FOOD_CAKE,  TMZ_JOB_WORKSHOP, 0x63D9A8, 0x87C93C, 0 },
    { "EMBER",  "EMBER",  TMZ_FOOD_SOUP,   TMZ_FOOD_TEA,   TMZ_JOB_CAFE,     0xFF9D32, 0xFF5A36, 1 },
    { "BLOOP",  "BUBBLE", TMZ_FOOD_SOUP,   TMZ_FOOD_CAKE,  TMZ_JOB_CLINIC,   0x4ED7E8, 0xFF8BB8, 2 },
    { "ROOK",   "PEBBLE", TMZ_FOOD_RICE,   TMZ_FOOD_BERRY, TMZ_JOB_WORKSHOP, 0x87949B, 0xA59168, 3 },
    { "NIMBUS", "CLOUD",  TMZ_FOOD_TEA,    TMZ_FOOD_RICE,  TMZ_JOB_CAFE,     0xBFA7E8, 0x7A69B2, 4 },
    { "COGGO",  "GEAR",   TMZ_FOOD_RICE,   TMZ_FOOD_SOUP,  TMZ_JOB_WORKSHOP, 0xE8B53E, 0x9B6B22, 5 },
    { "NOVA",   "STAR",   TMZ_FOOD_BERRY,  TMZ_FOOD_CAKE,  TMZ_JOB_CLINIC,   0x34518A, 0xFFD34F, 6 },
    { "INKY",   "INK",    TMZ_FOOD_TEA,    TMZ_FOOD_GREENS,TMZ_JOB_WORKSHOP, 0x55458D, 0x9170C7, 7 },
    { "HONEY",  "BEEBEAR",TMZ_FOOD_CAKE,   TMZ_FOOD_GREENS,TMZ_JOB_CAFE,     0xF4C542, 0x8C5A25, 8 },
    { "TEMPO",  "DRUMFROG",TMZ_FOOD_BERRY, TMZ_FOOD_TEA,   TMZ_JOB_CAFE,     0xE96856, 0xF4C86A, 9 },
    { "LUNA",   "MOON",   TMZ_FOOD_TEA,    TMZ_FOOD_SOUP,  TMZ_JOB_CLINIC,   0x744C91, 0xE4B85B, 10 },
    { "ZIP",    "GECKO",  TMZ_FOOD_RICE,   TMZ_FOOD_CAKE,  TMZ_JOB_COURIER,  0x8BCB3F, 0xF2E34E, 11 },
};

static const tmz_food_def_t FOODS[TMZ_FOOD_COUNT] = {
    { "BASIC",  0, 24,  1,  1, 40 },
    { "GREENS", 4, 28,  5,  0, 35 },
    { "SOUP",   6, 24,  8,  0, 30 },
    { "RICE",   8, 36,  3,  2, 55 },
    { "BERRY",  4, 12, 12,  0, 25 },
    { "CAKE",  10, 10, 20,  4, 45 },
    { "TEA",    5,  8, 10, -1, 20 },
};

static const tmz_item_def_t ITEMS[TMZ_ITEM_COUNT] = {
    { "GREENS", 4, true },
    { "SOUP", 6, true },
    { "RICE", 8, true },
    { "BERRY", 4, true },
    { "CAKE", 10, true },
    { "TEA", 5, true },
    { "TOY", 18, false },
    { "BOOK", 16, false },
    { "GYM", 22, false },
    { "DECOR", 30, false },
};

static const char *const STAGE_NAMES[] = { "BABY", "CHILD", "TEEN", "ADULT" };
static const char *const ILLNESS_NAMES[] = { "HEALTHY", "COLD", "STOMACH", "FATIGUE", "DIRT" };
static const char *const JOB_NAMES[] = { "CAFE", "COURIER", "WORKSHOP", "CLINIC" };
static const char *const RESULT_NAMES[] = {
    "OK", "NOTHING", "LOCKED", "NEED COINS", "NEED ITEM",
    "NEED ENERGY", "NEED HEALTH", "NEED BOND", "COOLDOWN",
    "FULL", "INVALID",
};

uint8_t tmz_clamp_percent(int value)
{
    if (value < 0) return 0;
    if (value > 100) return 100;
    return (uint8_t)value;
}

static uint32_t random_next(tmz_model_t *model)
{
    uint32_t x = model->rng ? model->rng : 0xA341316CU;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    model->rng = x;
    return x;
}

static void adjust(uint8_t *value, int delta)
{
    *value = tmz_clamp_percent((int)*value + delta);
}

static uint16_t award_regular_coins(tmz_model_t *model, uint16_t requested)
{
    if (!model || requested == 0 || model->income_this_day >= TMZ_DAILY_COIN_CAP) return 0;
    uint16_t room = (uint16_t)(TMZ_DAILY_COIN_CAP - model->income_this_day);
    uint16_t awarded = requested < room ? requested : room;
    model->coins = (uint16_t)(model->coins + awarded);
    model->income_this_day = (uint16_t)(model->income_this_day + awarded);
    model->total_coins_earned = (uint16_t)(model->total_coins_earned + awarded);
    return awarded;
}

static void award_milestone_coins(tmz_model_t *model, uint16_t amount)
{
    model->coins = (uint16_t)(model->coins + amount);
    model->total_coins_earned = (uint16_t)(model->total_coins_earned + amount);
}

const tmz_pet_def_t *tmz_pet_def(tmz_pet_id_t id)
{
    return (unsigned)id < TMZ_PET_COUNT ? &PETS[id] : NULL;
}

const tmz_food_def_t *tmz_food_def(tmz_food_t food)
{
    return (unsigned)food < TMZ_FOOD_COUNT ? &FOODS[food] : NULL;
}

const tmz_item_def_t *tmz_item_def(tmz_item_t item)
{
    return (unsigned)item < TMZ_ITEM_COUNT ? &ITEMS[item] : NULL;
}

const char *tmz_stage_name(tmz_stage_t stage)
{
    return (unsigned)stage < 4U ? STAGE_NAMES[stage] : "?";
}

const char *tmz_illness_name(tmz_illness_t illness)
{
    return (unsigned)illness < 5U ? ILLNESS_NAMES[illness] : "?";
}

const char *tmz_job_name(tmz_job_t job)
{
    return (unsigned)job < TMZ_JOB_COUNT ? JOB_NAMES[job] : "?";
}

const char *tmz_result_name(tmz_result_t result)
{
    return (unsigned)result < sizeof(RESULT_NAMES) / sizeof(RESULT_NAMES[0]) ?
           RESULT_NAMES[result] : "?";
}

void tmz_model_init(tmz_model_t *model, uint32_t seed)
{
    if (!model) return;
    memset(model, 0, sizeof(*model));
    model->version = TMZ_MODEL_VERSION;
    model->rng = seed ? seed : 0x544D5A31U;
    model->coins = 30;
    model->sound_level = 2;
    model->inventory[TMZ_ITEM_GREENS] = 2;
    model->inventory[TMZ_ITEM_SOUP] = 1;
    model->inventory[TMZ_ITEM_BERRY] = 1;
}

static uint8_t next_generation(const tmz_model_t *model)
{
    uint8_t generation = 1;
    for (uint8_t i = 0; i < model->archive_count; i++) {
        if (model->archive[i].generation >= generation) {
            generation = (uint8_t)(model->archive[i].generation + 1U);
        }
    }
    return generation;
}

static void reset_pet(tmz_model_t *model, tmz_pet_id_t species,
                      tmz_pet_id_t accent, uint8_t generation,
                      uint8_t personality, tmz_job_t talent)
{
    model->active = true;
    model->species = (uint8_t)species;
    model->accent_species = (uint8_t)accent;
    model->stage = TMZ_STAGE_BABY;
    model->illness = TMZ_ILLNESS_NONE;
    model->personality = personality;
    model->talent_job = (uint8_t)talent;
    model->generation = generation;
    model->fullness = 80;
    model->mood = 75;
    model->cleanliness = 90;
    model->health = 100;
    model->energy = 85;
    model->knowledge = 0;
    model->fitness = 0;
    model->manners = 0;
    model->bond = 10;
    model->weight = 20;
    model->waste_count = 0;
    model->care_mistakes = 0;
    model->digestion_minutes = 0;
    model->toilet_urge_minutes = 0;
    model->work_cooldown_minutes = 0;
    model->pet_age_minutes = 0;
    model->second_accumulator = 0;
    model->badge_mask = 0;
}

tmz_result_t tmz_model_start_pet(tmz_model_t *model, tmz_pet_id_t species)
{
    const tmz_pet_def_t *def = tmz_pet_def(species);
    if (!model || !def || model->version != TMZ_MODEL_VERSION) return TMZ_RESULT_INVALID;
    if (model->active) return TMZ_RESULT_LOCKED;
    reset_pet(model, species, species, next_generation(model),
              (uint8_t)(random_next(model) % 6U), def->talent_job);
    return TMZ_RESULT_OK;
}

bool tmz_model_sanitize(tmz_model_t *model)
{
    if (!model || model->version != TMZ_MODEL_VERSION) return false;
    if (model->species >= TMZ_PET_COUNT) model->species = TMZ_PET_SPRIG;
    if (model->accent_species >= TMZ_PET_COUNT) model->accent_species = model->species;
    if (model->stage > TMZ_STAGE_ADULT) model->stage = TMZ_STAGE_BABY;
    if (model->illness > TMZ_ILLNESS_DIRT) model->illness = TMZ_ILLNESS_NONE;
    if (model->talent_job >= TMZ_JOB_COUNT) model->talent_job = tmz_pet_def((tmz_pet_id_t)model->species)->talent_job;
    model->fullness = tmz_clamp_percent(model->fullness);
    model->mood = tmz_clamp_percent(model->mood);
    model->cleanliness = tmz_clamp_percent(model->cleanliness);
    model->health = tmz_clamp_percent(model->health);
    model->energy = tmz_clamp_percent(model->energy);
    model->knowledge = tmz_clamp_percent(model->knowledge);
    model->fitness = tmz_clamp_percent(model->fitness);
    model->manners = tmz_clamp_percent(model->manners);
    model->bond = tmz_clamp_percent(model->bond);
    if (model->archive_count > TMZ_ARCHIVE_MAX) model->archive_count = TMZ_ARCHIVE_MAX;
    if (model->sound_level > 3) model->sound_level = 2;
    for (size_t i = 0; i < TMZ_ITEM_COUNT; i++) {
        if (model->inventory[i] > 9) model->inventory[i] = 9;
    }
    return true;
}

static uint32_t advance_one_minute(tmz_model_t *model)
{
    uint32_t events = TMZ_EVENT_NONE;
    uint8_t old_fullness = model->fullness;
    tmz_stage_t old_stage = (tmz_stage_t)model->stage;

    model->pet_age_minutes++;
    model->pet_day_minutes++;
    if (model->pet_day_minutes >= TMZ_PET_DAY_MINUTES) {
        model->pet_day_minutes = 0;
        model->income_this_day = 0;
        model->timely_toilet_claimed = 0;
        events |= TMZ_EVENT_PET_DAY;
    }
    if (model->work_cooldown_minutes > 0) model->work_cooldown_minutes--;

    if (model->digestion_minutes > 0 && --model->digestion_minutes == 0) {
        model->toilet_urge_minutes = 5;
        events |= TMZ_EVENT_TOILET_URGE;
    }
    if (model->toilet_urge_minutes > 0 && --model->toilet_urge_minutes == 0) {
        if (model->waste_count < 3) model->waste_count++;
        adjust(&model->cleanliness, -25);
        adjust(&model->manners, -2);
        model->care_mistakes++;
        events |= TMZ_EVENT_WASTE;
    }

    if (model->pet_age_minutes % 10U == 0) {
        adjust(&model->fullness, -3);
        adjust(&model->mood, -1);
        adjust(&model->energy, -1);
        if (model->waste_count > 0) adjust(&model->cleanliness, -(int)model->waste_count);
        if (model->fullness == 0) adjust(&model->health, -4);
        if (model->cleanliness < 20) adjust(&model->health, -2);
        if (model->health > 0 && model->illness == TMZ_ILLNESS_NONE &&
            model->fullness > 45 && model->cleanliness > 45 &&
            model->pet_age_minutes % 30U == 0) {
            adjust(&model->health, 1);
            adjust(&model->bond, 1);
        }
    }

    if (old_fullness >= 20 && model->fullness < 20) {
        model->care_mistakes++;
        events |= TMZ_EVENT_HUNGRY;
    }

    if (model->illness == TMZ_ILLNESS_NONE && model->pet_age_minutes % 30U == 0) {
        if (model->cleanliness < 25 && random_next(model) % 3U == 0) {
            model->illness = TMZ_ILLNESS_DIRT;
        } else if (model->fullness < 10 && random_next(model) % 4U == 0) {
            model->illness = TMZ_ILLNESS_STOMACH;
        } else if (model->energy < 10 && random_next(model) % 4U == 0) {
            model->illness = TMZ_ILLNESS_FATIGUE;
        } else if (model->health < 45 && random_next(model) % 5U == 0) {
            model->illness = TMZ_ILLNESS_COLD;
        }
        if (model->illness != TMZ_ILLNESS_NONE) {
            adjust(&model->health, -10);
            events |= TMZ_EVENT_SICK;
        }
    }

    if (model->pet_age_minutes >= 990U) model->stage = TMZ_STAGE_ADULT;
    else if (model->pet_age_minutes >= 270U) model->stage = TMZ_STAGE_TEEN;
    else if (model->pet_age_minutes >= 30U) model->stage = TMZ_STAGE_CHILD;
    else model->stage = TMZ_STAGE_BABY;

    if (old_stage != (tmz_stage_t)model->stage) {
        adjust(&model->bond, 5);
        model->badge_mask |= (uint8_t)(1U << model->stage);
        award_milestone_coins(model, (uint16_t)(10U + 5U * model->stage));
        events |= TMZ_EVENT_GROW;
    }
    if (model->health < 20) events |= TMZ_EVENT_CRITICAL;
    return events;
}

uint32_t tmz_model_tick(tmz_model_t *model, uint32_t elapsed_seconds)
{
    if (!model || !model->active || model->version != TMZ_MODEL_VERSION) return TMZ_EVENT_NONE;
    uint32_t events = TMZ_EVENT_NONE;
    uint32_t total = (uint32_t)model->second_accumulator + elapsed_seconds;
    uint32_t minutes = total / 60U;
    model->second_accumulator = (uint16_t)(total % 60U);
    if (minutes > 1440U) minutes = 1440U;
    for (uint32_t i = 0; i < minutes; i++) events |= advance_one_minute(model);
    return events;
}

tmz_result_t tmz_model_feed(tmz_model_t *model, tmz_food_t food)
{
    const tmz_food_def_t *food_def = tmz_food_def(food);
    if (!model || !model->active || !food_def) return TMZ_RESULT_INVALID;
    if (model->fullness >= 98 && food != TMZ_FOOD_TEA) return TMZ_RESULT_FULL;
    if (food != TMZ_FOOD_BASIC) {
        size_t item = (size_t)food - 1U;
        if (item >= TMZ_ITEM_COUNT || model->inventory[item] == 0) return TMZ_RESULT_NEED_ITEM;
        model->inventory[item]--;
    }

    const tmz_pet_def_t *pet = tmz_pet_def((tmz_pet_id_t)model->species);
    int mood = food_def->mood;
    if (food == pet->favorite_food) mood += 8;
    if (food == pet->disliked_food) mood -= 6;
    adjust(&model->fullness, food_def->fullness);
    adjust(&model->mood, mood);
    adjust(&model->bond, food == pet->favorite_food ? 2 : 1);
    model->weight = (uint8_t)((int)model->weight + food_def->weight < 1 ? 1 :
                             (int)model->weight + food_def->weight > 99 ? 99 :
                             (int)model->weight + food_def->weight);
    if (model->digestion_minutes == 0) model->digestion_minutes = food_def->digestion_minutes;
    else if (model->digestion_minutes < 120) model->digestion_minutes += food_def->digestion_minutes / 3U;
    return TMZ_RESULT_OK;
}

tmz_result_t tmz_model_toilet(tmz_model_t *model, uint16_t *coins_awarded)
{
    if (coins_awarded) *coins_awarded = 0;
    if (!model || !model->active) return TMZ_RESULT_INVALID;
    if (model->toilet_urge_minutes == 0) return TMZ_RESULT_NOTHING;
    model->toilet_urge_minutes = 0;
    adjust(&model->cleanliness, 8);
    adjust(&model->manners, 3);
    adjust(&model->bond, 1);
    if (!model->timely_toilet_claimed) {
        uint16_t amount = award_regular_coins(model, 2);
        model->timely_toilet_claimed = 1;
        if (coins_awarded) *coins_awarded = amount;
    }
    return TMZ_RESULT_OK;
}

tmz_result_t tmz_model_sweep(tmz_model_t *model)
{
    if (!model || !model->active) return TMZ_RESULT_INVALID;
    if (model->waste_count == 0) return TMZ_RESULT_NOTHING;
    adjust(&model->cleanliness, 24 * model->waste_count);
    model->waste_count = 0;
    adjust(&model->bond, 1);
    return TMZ_RESULT_OK;
}

tmz_result_t tmz_model_bath(tmz_model_t *model)
{
    if (!model || !model->active) return TMZ_RESULT_INVALID;
    if (model->cleanliness >= 95) return TMZ_RESULT_NOTHING;
    adjust(&model->cleanliness, 35);
    adjust(&model->energy, -2);
    adjust(&model->mood, 3);
    return TMZ_RESULT_OK;
}

tmz_result_t tmz_model_rest(tmz_model_t *model)
{
    if (!model || !model->active) return TMZ_RESULT_INVALID;
    if (model->energy >= 95 && model->health >= 95) return TMZ_RESULT_NOTHING;
    adjust(&model->energy, 32);
    adjust(&model->health, 5);
    adjust(&model->mood, 2);
    if (model->illness == TMZ_ILLNESS_FATIGUE && model->energy >= 60) {
        model->illness = TMZ_ILLNESS_NONE;
    }
    return TMZ_RESULT_OK;
}

tmz_result_t tmz_model_treat(tmz_model_t *model, tmz_treatment_t treatment)
{
    if (!model || !model->active || (unsigned)treatment > TMZ_TREAT_CLEAN) return TMZ_RESULT_INVALID;
    if (treatment == TMZ_TREAT_REST) return tmz_model_rest(model);
    if (treatment == TMZ_TREAT_CLEAN) {
        tmz_result_t result = tmz_model_sweep(model);
        if (model->cleanliness < 95 && tmz_model_bath(model) == TMZ_RESULT_OK) {
            result = TMZ_RESULT_OK;
        }
        if (model->illness == TMZ_ILLNESS_DIRT && model->cleanliness >= 60) {
            model->illness = TMZ_ILLNESS_NONE;
            adjust(&model->health, 18);
            return TMZ_RESULT_OK;
        }
        return result;
    }
    if (treatment == TMZ_TREAT_SOUP) {
        tmz_result_t result = tmz_model_feed(model, TMZ_FOOD_SOUP);
        if (result != TMZ_RESULT_OK) return result;
        if (model->illness == TMZ_ILLNESS_STOMACH || model->illness == TMZ_ILLNESS_COLD) {
            model->illness = TMZ_ILLNESS_NONE;
            adjust(&model->health, 15);
        }
        return TMZ_RESULT_OK;
    }
    if (model->illness == TMZ_ILLNESS_NONE) return TMZ_RESULT_NOTHING;
    if (model->illness == TMZ_ILLNESS_FATIGUE) {
        adjust(&model->health, 4);
        return TMZ_RESULT_NOTHING;
    }
    model->illness = TMZ_ILLNESS_NONE;
    adjust(&model->health, 24);
    adjust(&model->bond, 2);
    return TMZ_RESULT_OK;
}

static tmz_result_t activity_ready(const tmz_model_t *model, uint8_t energy,
                                   tmz_stage_t stage)
{
    if (!model || !model->active) return TMZ_RESULT_INVALID;
    if (model->stage < stage) return TMZ_RESULT_LOCKED;
    if (model->illness != TMZ_ILLNESS_NONE || model->health < 35) return TMZ_RESULT_NEED_HEALTH;
    if (model->energy < energy) return TMZ_RESULT_NEED_ENERGY;
    return TMZ_RESULT_OK;
}

tmz_result_t tmz_model_train(tmz_model_t *model, uint8_t score,
                             uint16_t *coins_awarded)
{
    if (coins_awarded) *coins_awarded = 0;
    tmz_result_t ready = activity_ready(model, 15, TMZ_STAGE_CHILD);
    if (ready != TMZ_RESULT_OK) return ready;
    if (score > 100) score = 100;
    adjust(&model->energy, -15);
    adjust(&model->fitness, 1 + score / 20);
    adjust(&model->mood, 2 + score / 25);
    adjust(&model->bond, 1);
    if (model->weight > 5) model->weight--;
    uint16_t amount = award_regular_coins(model, (uint16_t)(2U + score * 6U / 100U));
    if (tmz_model_item_owned(model, TMZ_ITEM_GYM)) adjust(&model->fitness, 1);
    if (coins_awarded) *coins_awarded = amount;
    return TMZ_RESULT_OK;
}

tmz_result_t tmz_model_learn(tmz_model_t *model, uint8_t score,
                             uint16_t *coins_awarded)
{
    if (coins_awarded) *coins_awarded = 0;
    tmz_result_t ready = activity_ready(model, 8, TMZ_STAGE_CHILD);
    if (ready != TMZ_RESULT_OK) return ready;
    if (score > 100) score = 100;
    adjust(&model->energy, -8);
    adjust(&model->knowledge, 1 + score / 20);
    adjust(&model->manners, score >= 75 ? 2 : 0);
    adjust(&model->mood, score >= 50 ? 3 : 1);
    adjust(&model->bond, 1);
    uint16_t amount = award_regular_coins(model, (uint16_t)(2U + score * 6U / 100U));
    if (tmz_model_item_owned(model, TMZ_ITEM_BOOK)) adjust(&model->knowledge, 1);
    if (coins_awarded) *coins_awarded = amount;
    return TMZ_RESULT_OK;
}

tmz_result_t tmz_model_work(tmz_model_t *model, tmz_job_t job, uint8_t score,
                            uint16_t *coins_awarded)
{
    static const uint8_t BASE_MIN[] = { 6, 6, 7, 7 };
    static const uint8_t BASE_MAX[] = { 12, 14, 15, 15 };
    if (coins_awarded) *coins_awarded = 0;
    if ((unsigned)job >= TMZ_JOB_COUNT) return TMZ_RESULT_INVALID;
    tmz_result_t ready = activity_ready(model, 20, TMZ_STAGE_TEEN);
    if (ready != TMZ_RESULT_OK) return ready;
    if (model->work_cooldown_minutes > 0) return TMZ_RESULT_COOLDOWN;
    if (score > 100) score = 100;
    adjust(&model->energy, -20);
    adjust(&model->mood, 1 + score / 33);
    adjust(&model->bond, 1);
    uint16_t amount = (uint16_t)(BASE_MIN[job] +
                      (BASE_MAX[job] - BASE_MIN[job]) * score / 100U);
    if (model->talent_job == (uint8_t)job) amount = (uint16_t)(amount + (amount + 9U) / 10U);
    if (model->species == TMZ_PET_HONEY) amount = (uint16_t)(amount + (amount + 19U) / 20U);
    amount = award_regular_coins(model, amount);
    model->work_cooldown_minutes = 30;
    if (coins_awarded) *coins_awarded = amount;
    return TMZ_RESULT_OK;
}

tmz_result_t tmz_model_buy_item(tmz_model_t *model, tmz_item_t item)
{
    const tmz_item_def_t *def = tmz_item_def(item);
    if (!model || !model->active || !def) return TMZ_RESULT_INVALID;
    if (def->consumable) {
        if (model->inventory[item] >= 9) return TMZ_RESULT_FULL;
    } else {
        uint16_t bit = (uint16_t)(1U << item);
        if (model->permanent_items & bit) return TMZ_RESULT_FULL;
    }
    if (model->coins < def->price) return TMZ_RESULT_NEED_COINS;
    if (def->consumable) {
        model->inventory[item]++;
    } else {
        uint16_t bit = (uint16_t)(1U << item);
        model->permanent_items |= bit;
    }
    model->coins = (uint16_t)(model->coins - def->price);
    return TMZ_RESULT_OK;
}

bool tmz_model_item_owned(const tmz_model_t *model, tmz_item_t item)
{
    if (!model || (unsigned)item >= TMZ_ITEM_COUNT) return false;
    if (ITEMS[item].consumable) return model->inventory[item] > 0;
    return (model->permanent_items & (uint16_t)(1U << item)) != 0;
}

bool tmz_model_can_archive(const tmz_model_t *model)
{
    return model && model->active && model->stage == TMZ_STAGE_ADULT && model->bond >= 60;
}

static void archive_push(tmz_model_t *model, const tmz_gene_t *gene)
{
    if (model->archive_count >= TMZ_ARCHIVE_MAX) {
        memmove(&model->archive[0], &model->archive[1],
                sizeof(model->archive[0]) * (TMZ_ARCHIVE_MAX - 1U));
        model->archive_count = TMZ_ARCHIVE_MAX - 1U;
    }
    model->archive[model->archive_count++] = *gene;
}

static tmz_gene_t current_gene(const tmz_model_t *model)
{
    tmz_gene_t gene = {
        .species = model->species,
        .accent_species = model->accent_species,
        .personality = model->personality,
        .talent_job = model->talent_job,
        .generation = model->generation,
        .badge_mask = model->badge_mask,
    };
    return gene;
}

tmz_result_t tmz_model_archive_current(tmz_model_t *model)
{
    if (!model || !model->active) return TMZ_RESULT_INVALID;
    if (model->stage != TMZ_STAGE_ADULT) return TMZ_RESULT_LOCKED;
    if (model->bond < 60) return TMZ_RESULT_NEED_BOND;
    tmz_gene_t gene = current_gene(model);
    archive_push(model, &gene);
    award_milestone_coins(model, 30);
    model->active = false;
    return TMZ_RESULT_OK;
}

tmz_result_t tmz_model_fuse(tmz_model_t *model, size_t archive_index)
{
    if (!model || !model->active || archive_index >= model->archive_count) return TMZ_RESULT_INVALID;
    if (model->stage != TMZ_STAGE_ADULT) return TMZ_RESULT_LOCKED;
    if (model->bond < 80) return TMZ_RESULT_NEED_BOND;
    uint16_t cost = model->fusion_count == 0 ? 0 : 30;
    if (model->coins < cost) return TMZ_RESULT_NEED_COINS;

    tmz_gene_t memory = model->archive[archive_index];
    tmz_gene_t current = current_gene(model);
    uint8_t generation = current.generation > memory.generation ? current.generation : memory.generation;
    if (generation < 255) generation++;
    uint8_t personality = (uint8_t)((current.personality + memory.personality +
                                     random_next(model)) % 6U);
    tmz_job_t talent = (random_next(model) & 1U) ?
                       (tmz_job_t)current.talent_job : (tmz_job_t)memory.talent_job;
    model->coins = (uint16_t)(model->coins - cost);
    archive_push(model, &current);
    model->fusion_count++;
    reset_pet(model, (tmz_pet_id_t)current.species,
              (tmz_pet_id_t)memory.species, generation, personality, talent);
    model->bond = 15;
    return TMZ_RESULT_OK;
}
