#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TMZ_MODEL_VERSION 1U
#define TMZ_PET_COUNT 12U
#define TMZ_ARCHIVE_MAX 12U
#define TMZ_PET_DAY_MINUTES 240U
#define TMZ_DAILY_COIN_CAP 60U

typedef enum {
    TMZ_PET_SPRIG = 0,
    TMZ_PET_EMBER,
    TMZ_PET_BLOOP,
    TMZ_PET_ROOK,
    TMZ_PET_NIMBUS,
    TMZ_PET_COGGO,
    TMZ_PET_NOVA,
    TMZ_PET_INKY,
    TMZ_PET_HONEY,
    TMZ_PET_TEMPO,
    TMZ_PET_LUNA,
    TMZ_PET_ZIP,
} tmz_pet_id_t;

typedef enum {
    TMZ_STAGE_BABY = 0,
    TMZ_STAGE_CHILD,
    TMZ_STAGE_TEEN,
    TMZ_STAGE_ADULT,
} tmz_stage_t;

typedef enum {
    TMZ_ILLNESS_NONE = 0,
    TMZ_ILLNESS_COLD,
    TMZ_ILLNESS_STOMACH,
    TMZ_ILLNESS_FATIGUE,
    TMZ_ILLNESS_DIRT,
} tmz_illness_t;

typedef enum {
    TMZ_FOOD_BASIC = 0,
    TMZ_FOOD_GREENS,
    TMZ_FOOD_SOUP,
    TMZ_FOOD_RICE,
    TMZ_FOOD_BERRY,
    TMZ_FOOD_CAKE,
    TMZ_FOOD_TEA,
    TMZ_FOOD_COUNT,
} tmz_food_t;

typedef enum {
    TMZ_ITEM_GREENS = 0,
    TMZ_ITEM_SOUP,
    TMZ_ITEM_RICE,
    TMZ_ITEM_BERRY,
    TMZ_ITEM_CAKE,
    TMZ_ITEM_TEA,
    TMZ_ITEM_TOY,
    TMZ_ITEM_BOOK,
    TMZ_ITEM_GYM,
    TMZ_ITEM_DECOR,
    TMZ_ITEM_COUNT,
} tmz_item_t;

typedef enum {
    TMZ_JOB_CAFE = 0,
    TMZ_JOB_COURIER,
    TMZ_JOB_WORKSHOP,
    TMZ_JOB_CLINIC,
    TMZ_JOB_COUNT,
} tmz_job_t;

typedef enum {
    TMZ_TREAT_REST = 0,
    TMZ_TREAT_MEDICINE,
    TMZ_TREAT_SOUP,
    TMZ_TREAT_CLEAN,
} tmz_treatment_t;

typedef enum {
    TMZ_RESULT_OK = 0,
    TMZ_RESULT_NOTHING,
    TMZ_RESULT_LOCKED,
    TMZ_RESULT_NEED_COINS,
    TMZ_RESULT_NEED_ITEM,
    TMZ_RESULT_NEED_ENERGY,
    TMZ_RESULT_NEED_HEALTH,
    TMZ_RESULT_NEED_BOND,
    TMZ_RESULT_COOLDOWN,
    TMZ_RESULT_FULL,
    TMZ_RESULT_INVALID,
} tmz_result_t;

enum {
    TMZ_EVENT_NONE = 0,
    TMZ_EVENT_HUNGRY = 1U << 0,
    TMZ_EVENT_TOILET_URGE = 1U << 1,
    TMZ_EVENT_WASTE = 1U << 2,
    TMZ_EVENT_SICK = 1U << 3,
    TMZ_EVENT_GROW = 1U << 4,
    TMZ_EVENT_CRITICAL = 1U << 5,
    TMZ_EVENT_RECOVERED = 1U << 6,
    TMZ_EVENT_PET_DAY = 1U << 7,
};

typedef struct {
    const char *name;
    const char *kind;
    tmz_food_t favorite_food;
    tmz_food_t disliked_food;
    tmz_job_t talent_job;
    uint32_t body_color;
    uint32_t accent_color;
    uint8_t sound_motif;
} tmz_pet_def_t;

typedef struct {
    const char *name;
    uint8_t price;
    uint8_t fullness;
    uint8_t mood;
    int8_t weight;
    uint8_t digestion_minutes;
} tmz_food_def_t;

typedef struct {
    const char *name;
    uint8_t price;
    bool consumable;
} tmz_item_def_t;

typedef struct {
    uint8_t species;
    uint8_t accent_species;
    uint8_t personality;
    uint8_t talent_job;
    uint8_t generation;
    uint8_t badge_mask;
} tmz_gene_t;

typedef struct {
    uint32_t version;
    uint32_t rng;
    bool active;
    uint8_t species;
    uint8_t accent_species;
    uint8_t stage;
    uint8_t illness;
    uint8_t personality;
    uint8_t talent_job;
    uint8_t generation;
    uint8_t fullness;
    uint8_t mood;
    uint8_t cleanliness;
    uint8_t health;
    uint8_t energy;
    uint8_t knowledge;
    uint8_t fitness;
    uint8_t manners;
    uint8_t bond;
    uint8_t weight;
    uint8_t waste_count;
    uint8_t inventory[TMZ_ITEM_COUNT];
    uint16_t permanent_items;
    uint16_t coins;
    uint16_t income_this_day;
    uint16_t total_coins_earned;
    uint16_t care_mistakes;
    uint16_t digestion_minutes;
    uint16_t toilet_urge_minutes;
    uint16_t work_cooldown_minutes;
    uint16_t pet_day_minutes;
    uint16_t second_accumulator;
    uint32_t pet_age_minutes;
    uint8_t timely_toilet_claimed;
    uint8_t badge_mask;
    uint8_t sound_level;
    uint8_t fusion_count;
    uint8_t archive_count;
    tmz_gene_t archive[TMZ_ARCHIVE_MAX];
} tmz_model_t;

const tmz_pet_def_t *tmz_pet_def(tmz_pet_id_t id);
const tmz_food_def_t *tmz_food_def(tmz_food_t food);
const tmz_item_def_t *tmz_item_def(tmz_item_t item);
const char *tmz_stage_name(tmz_stage_t stage);
const char *tmz_illness_name(tmz_illness_t illness);
const char *tmz_job_name(tmz_job_t job);
const char *tmz_result_name(tmz_result_t result);

void tmz_model_init(tmz_model_t *model, uint32_t seed);
tmz_result_t tmz_model_start_pet(tmz_model_t *model, tmz_pet_id_t species);
bool tmz_model_sanitize(tmz_model_t *model);
uint32_t tmz_model_tick(tmz_model_t *model, uint32_t elapsed_seconds);

tmz_result_t tmz_model_feed(tmz_model_t *model, tmz_food_t food);
tmz_result_t tmz_model_toilet(tmz_model_t *model, uint16_t *coins_awarded);
tmz_result_t tmz_model_sweep(tmz_model_t *model);
tmz_result_t tmz_model_bath(tmz_model_t *model);
tmz_result_t tmz_model_treat(tmz_model_t *model, tmz_treatment_t treatment);
tmz_result_t tmz_model_rest(tmz_model_t *model);
tmz_result_t tmz_model_train(tmz_model_t *model, uint8_t score,
                             uint16_t *coins_awarded);
tmz_result_t tmz_model_learn(tmz_model_t *model, uint8_t score,
                             uint16_t *coins_awarded);
tmz_result_t tmz_model_work(tmz_model_t *model, tmz_job_t job, uint8_t score,
                            uint16_t *coins_awarded);

tmz_result_t tmz_model_buy_item(tmz_model_t *model, tmz_item_t item);
bool tmz_model_item_owned(const tmz_model_t *model, tmz_item_t item);

bool tmz_model_can_archive(const tmz_model_t *model);
tmz_result_t tmz_model_archive_current(tmz_model_t *model);
tmz_result_t tmz_model_fuse(tmz_model_t *model, size_t archive_index);

uint8_t tmz_clamp_percent(int value);
