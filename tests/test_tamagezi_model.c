#include "tamagezi_model.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static tmz_model_t new_pet(tmz_pet_id_t species)
{
    tmz_model_t model;
    tmz_model_init(&model, 1234);
    assert(tmz_model_start_pet(&model, species) == TMZ_RESULT_OK);
    return model;
}

static void test_catalog(void)
{
    for (int i = 0; i < (int)TMZ_PET_COUNT; i++) {
        const tmz_pet_def_t *pet = tmz_pet_def((tmz_pet_id_t)i);
        assert(pet && pet->name && pet->kind);
        for (int j = i + 1; j < (int)TMZ_PET_COUNT; j++) {
            assert(strcmp(pet->name, tmz_pet_def((tmz_pet_id_t)j)->name) != 0);
        }
    }
    assert(tmz_pet_def((tmz_pet_id_t)TMZ_PET_COUNT) == NULL);
}

static void test_care_cycle(void)
{
    tmz_model_t model = new_pet(TMZ_PET_SPRIG);
    uint8_t greens = model.inventory[TMZ_ITEM_GREENS];
    assert(tmz_model_feed(&model, TMZ_FOOD_GREENS) == TMZ_RESULT_OK);
    assert(model.inventory[TMZ_ITEM_GREENS] == greens - 1);
    assert(model.digestion_minutes > 0);
    uint32_t events = tmz_model_tick(&model, model.digestion_minutes * 60U);
    assert(events & TMZ_EVENT_TOILET_URGE);
    uint16_t coins = 0;
    assert(tmz_model_toilet(&model, &coins) == TMZ_RESULT_OK);
    assert(coins == 2);

    model.waste_count = 2;
    model.cleanliness = 30;
    assert(tmz_model_sweep(&model) == TMZ_RESULT_OK);
    assert(model.waste_count == 0 && model.cleanliness > 30);
    model.cleanliness = 40;
    assert(tmz_model_bath(&model) == TMZ_RESULT_OK);
    assert(model.cleanliness == 75);
}

static void test_growth_and_activities(void)
{
    tmz_model_t model = new_pet(TMZ_PET_ZIP);
    uint16_t starting_coins = model.coins;
    assert(tmz_model_tick(&model, 30U * 60U) & TMZ_EVENT_GROW);
    assert(model.stage == TMZ_STAGE_CHILD && model.coins > starting_coins);
    uint16_t reward = 0;
    assert(tmz_model_train(&model, 100, &reward) == TMZ_RESULT_OK);
    assert(reward == 8 && model.fitness >= 6);
    model.energy = 100;
    assert(tmz_model_learn(&model, 75, &reward) == TMZ_RESULT_OK);
    assert(reward >= 6 && model.knowledge >= 4);

    assert(tmz_model_tick(&model, 240U * 60U) & TMZ_EVENT_GROW);
    assert(model.stage == TMZ_STAGE_TEEN);
    model.energy = 100;
    assert(tmz_model_work(&model, TMZ_JOB_COURIER, 100, &reward) == TMZ_RESULT_OK);
    assert(reward > 0 && model.work_cooldown_minutes == 30);
    assert(tmz_model_work(&model, TMZ_JOB_COURIER, 100, &reward) == TMZ_RESULT_COOLDOWN);
}

static void test_shop_treatment_and_cap(void)
{
    tmz_model_t model = new_pet(TMZ_PET_BLOOP);
    uint16_t coins = model.coins;
    assert(tmz_model_buy_item(&model, TMZ_ITEM_BOOK) == TMZ_RESULT_OK);
    assert(model.coins == coins - tmz_item_def(TMZ_ITEM_BOOK)->price);
    assert(tmz_model_item_owned(&model, TMZ_ITEM_BOOK));
    assert(tmz_model_buy_item(&model, TMZ_ITEM_BOOK) == TMZ_RESULT_FULL);

    model.illness = TMZ_ILLNESS_COLD;
    model.health = 50;
    assert(tmz_model_treat(&model, TMZ_TREAT_MEDICINE) == TMZ_RESULT_OK);
    assert(model.illness == TMZ_ILLNESS_NONE && model.health > 50);

    model.stage = TMZ_STAGE_CHILD;
    model.energy = 100;
    model.income_this_day = TMZ_DAILY_COIN_CAP - 1;
    uint16_t reward = 0;
    assert(tmz_model_train(&model, 100, &reward) == TMZ_RESULT_OK);
    assert(reward == 1 && model.income_this_day == TMZ_DAILY_COIN_CAP);
}

static void test_archive_and_fusion(void)
{
    tmz_model_t model = new_pet(TMZ_PET_NOVA);
    model.stage = TMZ_STAGE_ADULT;
    model.bond = 70;
    assert(tmz_model_archive_current(&model) == TMZ_RESULT_OK);
    assert(!model.active && model.archive_count == 1);
    assert(tmz_model_start_pet(&model, TMZ_PET_EMBER) == TMZ_RESULT_OK);
    model.stage = TMZ_STAGE_ADULT;
    model.bond = 80;
    assert(tmz_model_fuse(&model, 0) == TMZ_RESULT_OK);
    assert(model.active && model.stage == TMZ_STAGE_BABY);
    assert(model.species == TMZ_PET_EMBER && model.accent_species == TMZ_PET_NOVA);
    assert(model.archive_count == 2 && model.fusion_count == 1);
}

static void test_day_boundary_and_full_archive(void)
{
    tmz_model_t model = new_pet(TMZ_PET_ROOK);
    model.income_this_day = 42;
    model.timely_toilet_claimed = 1;
    assert(tmz_model_tick(&model, TMZ_PET_DAY_MINUTES * 60U) & TMZ_EVENT_PET_DAY);
    assert(model.income_this_day == 0 && model.timely_toilet_claimed == 0);

    for (int i = 0; i < (int)TMZ_ARCHIVE_MAX; i++) {
        model.archive[i].species = (uint8_t)i;
        model.archive[i].generation = 1;
    }
    model.archive_count = TMZ_ARCHIVE_MAX;
    model.stage = TMZ_STAGE_ADULT;
    model.bond = 100;
    assert(tmz_model_archive_current(&model) == TMZ_RESULT_OK);
    assert(model.archive_count == TMZ_ARCHIVE_MAX);
    assert(model.archive[0].species == TMZ_PET_EMBER);
    assert(model.archive[TMZ_ARCHIVE_MAX - 1].species == TMZ_PET_ROOK);
}

static void test_sanitize(void)
{
    tmz_model_t model = new_pet(TMZ_PET_INKY);
    model.species = 99;
    model.stage = 99;
    model.sound_level = 99;
    model.inventory[0] = 99;
    assert(tmz_model_sanitize(&model));
    assert(model.species == TMZ_PET_SPRIG && model.stage == TMZ_STAGE_BABY);
    assert(model.sound_level == 2 && model.inventory[0] == 9);
    model.version = 99;
    assert(!tmz_model_sanitize(&model));
}

int main(void)
{
    test_catalog();
    test_care_cycle();
    test_growth_and_activities();
    test_shop_treatment_and_cap();
    test_archive_and_fusion();
    test_day_boundary_and_full_archive();
    test_sanitize();
    puts("tamagezi model tests: PASS");
    return 0;
}
