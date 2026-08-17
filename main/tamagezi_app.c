#include "tamagezi_app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsp_battery.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lvgl.h"
#include "sdkconfig.h"
#include "tamagezi_audio.h"
#include "tamagezi_pet_view.h"
#include "tamagezi_store.h"
#include "ui_pixel.h"

typedef enum {
    SCREEN_SELECT = 0,
    SCREEN_HOME,
    SCREEN_STATUS,
    SCREEN_FEED,
    SCREEN_CLEAN,
    SCREEN_TREAT,
    SCREEN_TRAIN,
    SCREEN_LEARN,
    SCREEN_WORK,
    SCREEN_FUSION,
    SCREEN_PROFILE,
    SCREEN_SHOP,
} screen_t;

typedef struct {
    uint8_t button;
    uint8_t event;
} key_message_t;

static const char *const HOME_ACTIONS[] = {
    "STATUS", "FEED", "CLEAN", "TREAT", "TRAIN",
    "LEARN", "WORK", "FUSION", "PROFILE", "SHOP",
};
static const screen_t HOME_SCREENS[] = {
    SCREEN_STATUS, SCREEN_FEED, SCREEN_CLEAN, SCREEN_TREAT, SCREEN_TRAIN,
    SCREEN_LEARN, SCREEN_WORK, SCREEN_FUSION, SCREEN_PROFILE, SCREEN_SHOP,
};
static const char *const CLEAN_ACTIONS[] = { "TOILET", "SWEEP", "BATH" };
static const char *const TREAT_ACTIONS[] = { "REST", "MEDICINE", "SOUP", "CLEAN CARE" };

static tmz_model_t s_model;
static QueueHandle_t s_key_queue;
static lv_timer_t *s_timer;
static lv_obj_t *s_screen;
static lv_obj_t *s_game_marker;
static lv_obj_t *s_learn_label;
static screen_t s_current;
static int s_selection;
static int s_profile_page;
static bool s_battery_available;
static int s_battery = -1;
static int64_t s_last_us;
static int64_t s_tick_remainder_us;
static uint32_t s_save_seconds;
static uint32_t s_ui_seconds;
static char s_message[48];
static uint8_t s_game_position;
static int8_t s_game_direction;
static bool s_work_playing;
static tmz_job_t s_work_job;
static uint8_t s_learn_sequence[4];
static uint8_t s_learn_input;
static uint8_t s_learn_correct;
static int64_t s_learn_show_until;

static uint8_t motif(void)
{
    const tmz_pet_def_t *pet = tmz_pet_def((tmz_pet_id_t)s_model.species);
    return pet ? pet->sound_motif : 0;
}

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h,
                     uint32_t color, int radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_border_width(obj, 2, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(UI_INK), 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    return obj;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y,
                       const lv_font_t *font, uint32_t color)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    return obj;
}

static lv_obj_t *base_screen(const char *title)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xDDF4F3), 0);
    box(screen, 0, 0, 240, 44, UI_INK, 0);
    label(screen, title, 9, 8, &lv_font_montserrat_20, 0xFFFFFF);
    char header[32];
    if (s_battery_available && s_battery >= 0) {
        snprintf(header, sizeof(header), "$%u  %d%%", s_model.coins, s_battery);
    } else {
        snprintf(header, sizeof(header), "$%u", s_model.coins);
    }
    lv_obj_t *right = label(screen, header, 0, 13, &lv_font_montserrat_14, 0xFFD928);
    lv_obj_align(right, LV_ALIGN_TOP_RIGHT, -7, 0);
    label(screen, "UP/DOWN  OK:SELECT  HOLD:BACK", 4, 300,
          &lv_font_montserrat_14, 0x41545A);
    return screen;
}

static void load_screen(lv_obj_t *screen)
{
    lv_obj_t *old = s_screen;
    s_screen = screen;
    lv_screen_load(screen);
    if (old) lv_obj_delete(old);
}

static void list_row(lv_obj_t *screen, int row, const char *text, bool selected)
{
    lv_obj_t *panel = box(screen, 10, 51 + row * 38, 220, 32,
                          selected ? UI_YELLOW : 0xFFFFFF, 5);
    label(panel, text, 8, 6, &lv_font_montserrat_14, UI_INK);
}

static void build_list(const char *title, const char *const *items, int count)
{
    lv_obj_t *screen = base_screen(title);
    int first = s_selection > 5 ? s_selection - 5 : 0;
    for (int row = 0; row < 6 && first + row < count; row++) {
        list_row(screen, row, items[first + row], first + row == s_selection);
    }
    load_screen(screen);
}

static void build_select(void)
{
    const tmz_pet_def_t *pet = tmz_pet_def((tmz_pet_id_t)s_selection);
    lv_obj_t *screen = base_screen("CHOOSE A PET");
    tmz_model_t preview = s_model;
    preview.species = s_selection;
    preview.accent_species = s_selection;
    preview.illness = TMZ_ILLNESS_NONE;
    tmz_pet_view_create(screen, &preview, 62, 54, 116);
    lv_obj_t *name = label(screen, pet->name, 0, 180, &lv_font_montserrat_20, UI_INK);
    lv_obj_align(name, LV_ALIGN_TOP_MID, 0, 180);
    char info[64];
    snprintf(info, sizeof(info), "%02d/12  %s\nLIKES %s   TALENT %s",
             s_selection + 1, pet->kind, tmz_food_def(pet->favorite_food)->name,
             tmz_job_name(pet->talent_job));
    lv_obj_t *details = label(screen, info, 0, 218, &lv_font_montserrat_14, 0x41545A);
    lv_obj_set_style_text_align(details, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(details, LV_ALIGN_TOP_MID, 0, 218);
    load_screen(screen);
}

static void build_home(void)
{
    const tmz_pet_def_t *pet = tmz_pet_def((tmz_pet_id_t)s_model.species);
    lv_obj_t *screen = base_screen(pet->name);
    tmz_pet_view_create(screen, &s_model, 65, 48, 110);
    char stats[64];
    snprintf(stats, sizeof(stats), "%s  GEN %u   H%u M%u C%u E%u",
             tmz_stage_name((tmz_stage_t)s_model.stage), s_model.generation,
             s_model.fullness, s_model.mood, s_model.cleanliness, s_model.energy);
    lv_obj_t *stat = label(screen, stats, 0, 163, &lv_font_montserrat_14, 0x41545A);
    lv_obj_align(stat, LV_ALIGN_TOP_MID, 0, 163);
    box(screen, 13, 190, 214, 53, UI_YELLOW, 7);
    lv_obj_t *action = label(screen, HOME_ACTIONS[s_selection], 0, 201,
                             &lv_font_montserrat_20, UI_INK);
    lv_obj_align(action, LV_ALIGN_TOP_MID, 0, 201);
    char alerts[64] = "";
    if (s_model.toilet_urge_minutes) strcpy(alerts, "TOILET TIME!");
    else if (s_model.waste_count) snprintf(alerts, sizeof(alerts), "WASTE x%u", s_model.waste_count);
    else if (s_model.illness) snprintf(alerts, sizeof(alerts), "%s - NEEDS CARE", tmz_illness_name((tmz_illness_t)s_model.illness));
    else if (s_message[0]) snprintf(alerts, sizeof(alerts), "%s", s_message);
    else strcpy(alerts, "Your tiny friend is ready.");
    lv_obj_t *notice = label(screen, alerts, 0, 257, &lv_font_montserrat_14,
                             (s_model.illness || s_model.waste_count) ? UI_RED : 0x41545A);
    lv_obj_align(notice, LV_ALIGN_TOP_MID, 0, 257);
    load_screen(screen);
}

static void stat_line(lv_obj_t *screen, int row, const char *name, uint8_t value)
{
    char text[32];
    snprintf(text, sizeof(text), "%-10s %3u", name, value);
    label(screen, text, 20, 51 + row * 29, &lv_font_montserrat_14, UI_INK);
    box(screen, 118, 54 + row * 29, 102, 14, 0xFFFFFF, 4);
    int fill_width = value ? value * 98 / 100 : 1;
    lv_obj_t *fill = box(screen, 120, 56 + row * 29, fill_width, 10,
                         value < 25 ? UI_RED : UI_GRASS, 3);
    lv_obj_set_style_border_width(fill, 0, 0);
}

static void build_status(void)
{
    lv_obj_t *screen = base_screen("STATUS");
    stat_line(screen, 0, "FULLNESS", s_model.fullness);
    stat_line(screen, 1, "MOOD", s_model.mood);
    stat_line(screen, 2, "CLEAN", s_model.cleanliness);
    stat_line(screen, 3, "HEALTH", s_model.health);
    stat_line(screen, 4, "ENERGY", s_model.energy);
    stat_line(screen, 5, "BOND", s_model.bond);
    char bottom[80];
    snprintf(bottom, sizeof(bottom), "KNOW %u  FIT %u  MANNERS %u\n%s  AGE %lumin  WEIGHT %u",
             s_model.knowledge, s_model.fitness, s_model.manners,
             tmz_illness_name((tmz_illness_t)s_model.illness),
             (unsigned long)s_model.pet_age_minutes, s_model.weight);
    label(screen, bottom, 13, 234, &lv_font_montserrat_14, 0x41545A);
    load_screen(screen);
}

static void build_feed(void)
{
    const char *items[TMZ_FOOD_COUNT];
    char rows[TMZ_FOOD_COUNT][36];
    for (int i = 0; i < TMZ_FOOD_COUNT; i++) {
        const tmz_food_def_t *food = tmz_food_def((tmz_food_t)i);
        unsigned stock = i == 0 ? 99 : s_model.inventory[i - 1];
        snprintf(rows[i], sizeof(rows[i]), "%-9s x%u  +%u FULL", food->name, stock, food->fullness);
        items[i] = rows[i];
    }
    build_list("FEED", items, TMZ_FOOD_COUNT);
}

static void build_shop(void)
{
    const char *items[TMZ_ITEM_COUNT];
    char rows[TMZ_ITEM_COUNT][40];
    for (int i = 0; i < TMZ_ITEM_COUNT; i++) {
        const tmz_item_def_t *item = tmz_item_def((tmz_item_t)i);
        if (item->consumable) {
            snprintf(rows[i], sizeof(rows[i]), "%-9s $%u  x%u", item->name, item->price,
                     s_model.inventory[i]);
        } else {
            snprintf(rows[i], sizeof(rows[i]), "%-9s $%u  %s", item->name, item->price,
                     tmz_model_item_owned(&s_model, (tmz_item_t)i) ? "OWNED" : "");
        }
        items[i] = rows[i];
    }
    build_list("SHOP", items, TMZ_ITEM_COUNT);
}

static void build_train(void)
{
    lv_obj_t *screen = base_screen("TRAIN");
    label(screen, "Stop the marker near the center!", 13, 60,
          &lv_font_montserrat_14, UI_INK);
    box(screen, 18, 125, 204, 28, 0xFFFFFF, 4);
    box(screen, 103, 127, 34, 24, UI_GRASS, 2);
    s_game_marker = box(screen, 20 + s_game_position * 194 / 100, 119, 8, 40,
                        UI_RED, 2);
    label(screen, "OK = STOP", 74, 190, &lv_font_montserrat_20, UI_INK);
    label(screen, "Costs 15 energy. Unlocks at CHILD.", 13, 239,
          &lv_font_montserrat_14, 0x41545A);
    load_screen(screen);
}

static void learn_text(void)
{
    if (!s_learn_label) return;
    char text[80];
    if (esp_timer_get_time() < s_learn_show_until) {
        snprintf(text, sizeof(text), "MEMORIZE\n\n%c  %c  %c  %c",
                 s_learn_sequence[0] ? 'D' : 'U', s_learn_sequence[1] ? 'D' : 'U',
                 s_learn_sequence[2] ? 'D' : 'U', s_learn_sequence[3] ? 'D' : 'U');
    } else {
        snprintf(text, sizeof(text), "REPEAT WITH UP / DOWN\n\n%u / 4", s_learn_input);
    }
    lv_label_set_text(s_learn_label, text);
}

static void build_learn(void)
{
    lv_obj_t *screen = base_screen("LEARN");
    s_learn_label = label(screen, "", 0, 76, &lv_font_montserrat_20, UI_INK);
    lv_obj_set_width(s_learn_label, 240);
    lv_obj_set_style_text_align(s_learn_label, LV_TEXT_ALIGN_CENTER, 0);
    learn_text();
    label(screen, "Remember the four directions.", 16, 226,
          &lv_font_montserrat_14, 0x41545A);
    load_screen(screen);
}

static void build_work(void)
{
    if (!s_work_playing) {
        const char *items[TMZ_JOB_COUNT];
        for (int i = 0; i < TMZ_JOB_COUNT; i++) items[i] = tmz_job_name((tmz_job_t)i);
        build_list("WORK", items, TMZ_JOB_COUNT);
        return;
    }
    lv_obj_t *screen = base_screen(tmz_job_name(s_work_job));
    label(screen, "Hit the center to finish the job!", 13, 60,
          &lv_font_montserrat_14, UI_INK);
    box(screen, 18, 125, 204, 28, 0xFFFFFF, 4);
    box(screen, 103, 127, 34, 24, UI_YELLOW, 2);
    s_game_marker = box(screen, 20 + s_game_position * 194 / 100, 119, 8, 40,
                        UI_RED, 2);
    label(screen, "OK = WORK", 66, 190, &lv_font_montserrat_20, UI_INK);
    label(screen, "Costs 20 energy. 30-minute cooldown.", 9, 239,
          &lv_font_montserrat_14, 0x41545A);
    load_screen(screen);
}

static void build_fusion(void)
{
    const char *items[TMZ_ARCHIVE_MAX + 1];
    char rows[TMZ_ARCHIVE_MAX + 1][40];
    for (int i = 0; i < s_model.archive_count; i++) {
        const tmz_gene_t *gene = &s_model.archive[i];
        snprintf(rows[i], sizeof(rows[i]), "MEMORY %02d  %-8s G%u", i + 1,
                 tmz_pet_def((tmz_pet_id_t)gene->species)->name, gene->generation);
        items[i] = rows[i];
    }
    snprintf(rows[s_model.archive_count], sizeof(rows[0]), "ARCHIVE CURRENT PET");
    items[s_model.archive_count] = rows[s_model.archive_count];
    build_list("FUSION / LEGACY", items, s_model.archive_count + 1);
}

static void build_profile(void)
{
    const tmz_pet_def_t *pet = tmz_pet_def((tmz_pet_id_t)s_model.species);
    lv_obj_t *screen = base_screen("PROFILE");
    if (s_profile_page == 0) {
        tmz_pet_view_create(screen, &s_model, 12, 62, 88);
        char text[180];
        snprintf(text, sizeof(text), "%s\n%s / %s\nGENERATION %u\nPERSONALITY %u\nTALENT %s\nFUSIONS %u\nCARE MISTAKES %u",
                 pet->name, pet->kind, tmz_stage_name((tmz_stage_t)s_model.stage),
                 s_model.generation, s_model.personality + 1,
                 tmz_job_name((tmz_job_t)s_model.talent_job), s_model.fusion_count,
                 s_model.care_mistakes);
        label(screen, text, 105, 61, &lv_font_montserrat_14, UI_INK);
    } else {
        char text[260];
        snprintf(text, sizeof(text),
                 "RECORDS\n\nTOTAL COINS  %u\nARCHIVES     %u / %u\nBADGES       0x%02X\nACTIVE AGE   %lu min\nPET DAY      %u / %u\nDAILY INCOME %u / %u\n\nSound level: %u",
                 s_model.total_coins_earned, s_model.archive_count, TMZ_ARCHIVE_MAX,
                 s_model.badge_mask, (unsigned long)s_model.pet_age_minutes,
                 s_model.pet_day_minutes, TMZ_PET_DAY_MINUTES,
                 s_model.income_this_day, TMZ_DAILY_COIN_CAP, s_model.sound_level);
        label(screen, text, 25, 61, &lv_font_montserrat_14, UI_INK);
    }
    label(screen, "UP/DOWN changes page", 37, 267, &lv_font_montserrat_14, 0x41545A);
    load_screen(screen);
}

static void build_current(void)
{
    s_game_marker = NULL;
    s_learn_label = NULL;
    switch (s_current) {
    case SCREEN_SELECT: build_select(); break;
    case SCREEN_HOME: build_home(); break;
    case SCREEN_STATUS: build_status(); break;
    case SCREEN_FEED: build_feed(); break;
    case SCREEN_CLEAN: build_list("CLEAN", CLEAN_ACTIONS, 3); break;
    case SCREEN_TREAT: build_list("TREAT", TREAT_ACTIONS, 4); break;
    case SCREEN_TRAIN: build_train(); break;
    case SCREEN_LEARN: build_learn(); break;
    case SCREEN_WORK: build_work(); break;
    case SCREEN_FUSION: build_fusion(); break;
    case SCREEN_PROFILE: build_profile(); break;
    case SCREEN_SHOP: build_shop(); break;
    }
}

static void show_result(tmz_result_t result, uint16_t coins, tmz_sound_t success_sound)
{
    if (result == TMZ_RESULT_OK) {
        if (coins) snprintf(s_message, sizeof(s_message), "SUCCESS! +%u COINS", coins);
        else strcpy(s_message, "SUCCESS!");
        tmz_audio_play(coins ? TMZ_SOUND_COIN : success_sound, motif());
        tmz_store_save_async(&s_model);
    } else {
        snprintf(s_message, sizeof(s_message), "%s", tmz_result_name(result));
        tmz_audio_play(result == TMZ_RESULT_NOTHING ? TMZ_SOUND_MOVE : TMZ_SOUND_FAIL, motif());
    }
    s_current = s_model.active ? SCREEN_HOME : SCREEN_SELECT;
    s_selection = 0;
    build_current();
}

static void move_selection(int direction, int count)
{
    s_selection = (s_selection + direction + count) % count;
    tmz_audio_play(TMZ_SOUND_MOVE, motif());
    build_current();
}

static uint8_t timing_score(void)
{
    int distance = abs((int)s_game_position - 50);
    int score = 100 - distance * 2;
    return score < 0 ? 0 : (uint8_t)score;
}

static void enter_screen(screen_t screen)
{
    s_current = screen;
    s_selection = 0;
    s_game_position = 0;
    s_game_direction = 1;
    s_work_playing = false;
    if (screen == SCREEN_LEARN) {
        uint32_t random = esp_random();
        for (int i = 0; i < 4; i++) s_learn_sequence[i] = (random >> i) & 1U;
        s_learn_input = 0;
        s_learn_correct = 0;
        s_learn_show_until = esp_timer_get_time() + 2200000;
    }
    tmz_audio_play(TMZ_SOUND_SELECT, motif());
    build_current();
}

static void handle_ok(void)
{
    tmz_result_t result;
    uint16_t coins = 0;
    switch (s_current) {
    case SCREEN_SELECT:
        result = tmz_model_start_pet(&s_model, (tmz_pet_id_t)s_selection);
        show_result(result, 0, TMZ_SOUND_SUCCESS);
        break;
    case SCREEN_HOME:
        enter_screen(HOME_SCREENS[s_selection]);
        break;
    case SCREEN_STATUS:
    case SCREEN_PROFILE:
        break;
    case SCREEN_FEED:
        result = tmz_model_feed(&s_model, (tmz_food_t)s_selection);
        show_result(result, 0, TMZ_SOUND_FEED);
        break;
    case SCREEN_CLEAN:
        if (s_selection == 0) result = tmz_model_toilet(&s_model, &coins);
        else if (s_selection == 1) result = tmz_model_sweep(&s_model);
        else result = tmz_model_bath(&s_model);
        show_result(result, coins, TMZ_SOUND_CLEAN);
        break;
    case SCREEN_TREAT:
        result = tmz_model_treat(&s_model, (tmz_treatment_t)s_selection);
        show_result(result, 0, TMZ_SOUND_SUCCESS);
        break;
    case SCREEN_TRAIN:
        result = tmz_model_train(&s_model, timing_score(), &coins);
        show_result(result, coins, TMZ_SOUND_SUCCESS);
        break;
    case SCREEN_LEARN:
        break;
    case SCREEN_WORK:
        if (!s_work_playing) {
            s_work_job = (tmz_job_t)s_selection;
            s_work_playing = true;
            s_game_position = 0;
            s_game_direction = 1;
            build_current();
        } else {
            result = tmz_model_work(&s_model, s_work_job, timing_score(), &coins);
            show_result(result, coins, TMZ_SOUND_SUCCESS);
        }
        break;
    case SCREEN_FUSION:
        if (s_selection == s_model.archive_count) {
            result = tmz_model_archive_current(&s_model);
            show_result(result, 0, TMZ_SOUND_FUSION);
        } else {
            result = tmz_model_fuse(&s_model, s_selection);
            show_result(result, 0, TMZ_SOUND_FUSION);
        }
        break;
    case SCREEN_SHOP:
        result = tmz_model_buy_item(&s_model, (tmz_item_t)s_selection);
        show_result(result, 0, TMZ_SOUND_COIN);
        break;
    }
}

static int current_count(void)
{
    switch (s_current) {
    case SCREEN_HOME: return sizeof(HOME_ACTIONS) / sizeof(HOME_ACTIONS[0]);
    case SCREEN_SELECT: return TMZ_PET_COUNT;
    case SCREEN_FEED: return TMZ_FOOD_COUNT;
    case SCREEN_CLEAN: return 3;
    case SCREEN_TREAT: return 4;
    case SCREEN_WORK: return TMZ_JOB_COUNT;
    case SCREEN_FUSION: return s_model.archive_count + 1;
    case SCREEN_SHOP: return TMZ_ITEM_COUNT;
    default: return 0;
    }
}

static void handle_key(const key_message_t *key)
{
    bsp_btn_t button = (bsp_btn_t)key->button;
    bsp_btn_ev_t event = (bsp_btn_ev_t)key->event;
    if (button == BSP_BTN_OK && event == BSP_BTN_LONG) {
        if (s_current == SCREEN_HOME) {
            s_model.sound_level = (s_model.sound_level + 1U) % 4U;
            tmz_audio_set_level(s_model.sound_level);
            snprintf(s_message, sizeof(s_message), "SOUND LEVEL %u", s_model.sound_level);
            tmz_store_save_async(&s_model);
            build_current();
        } else if (s_current != SCREEN_SELECT) {
            s_current = SCREEN_HOME;
            s_selection = 0;
            build_current();
        }
        return;
    }
    if (s_current == SCREEN_LEARN && event == BSP_BTN_PRESS &&
        esp_timer_get_time() >= s_learn_show_until &&
        (button == BSP_BTN_UP || button == BSP_BTN_DOWN)) {
        uint8_t answer = button == BSP_BTN_DOWN;
        if (answer == s_learn_sequence[s_learn_input]) s_learn_correct++;
        s_learn_input++;
        tmz_audio_play(TMZ_SOUND_MOVE, motif());
        if (s_learn_input >= 4) {
            uint16_t coins = 0;
            tmz_result_t result = tmz_model_learn(&s_model, s_learn_correct * 25U, &coins);
            show_result(result, coins, TMZ_SOUND_SUCCESS);
        } else {
            learn_text();
        }
        return;
    }
    if (event != BSP_BTN_CLICK) return;
    if (s_current == SCREEN_PROFILE && (button == BSP_BTN_UP || button == BSP_BTN_DOWN)) {
        s_profile_page ^= 1;
        build_current();
        return;
    }
    int count = current_count();
    if (button == BSP_BTN_UP && count > 0 && !(s_current == SCREEN_WORK && s_work_playing)) {
        move_selection(-1, count);
    } else if (button == BSP_BTN_DOWN && count > 0 && !(s_current == SCREEN_WORK && s_work_playing)) {
        move_selection(1, count);
    } else if (button == BSP_BTN_OK) {
        handle_ok();
    }
}

static void timer_tick(lv_timer_t *timer)
{
    (void)timer;
    key_message_t key;
    while (xQueueReceive(s_key_queue, &key, 0) == pdTRUE) handle_key(&key);

    if ((s_current == SCREEN_TRAIN || (s_current == SCREEN_WORK && s_work_playing))) {
        int next = s_game_position + s_game_direction * 3;
        if (next >= 100) { next = 100; s_game_direction = -1; }
        if (next <= 0) { next = 0; s_game_direction = 1; }
        s_game_position = next;
        if (s_game_marker) lv_obj_set_x(s_game_marker, 20 + s_game_position * 194 / 100);
    }
    if (s_current == SCREEN_LEARN && s_learn_label) learn_text();

    int64_t now = esp_timer_get_time();
    int64_t elapsed = now - s_last_us;
    s_last_us = now;
    s_tick_remainder_us += elapsed;
    uint32_t seconds = s_tick_remainder_us / 1000000;
    s_tick_remainder_us %= 1000000;
    if (seconds > 0 && s_model.active) {
        uint32_t simulated_seconds = seconds * CONFIG_TAMAGEZI_TIME_SCALE;
        uint32_t events = tmz_model_tick(&s_model, simulated_seconds);
        s_save_seconds += simulated_seconds;
        s_ui_seconds += simulated_seconds;
        if (events) {
            if (events & TMZ_EVENT_GROW) tmz_audio_play(TMZ_SOUND_GROW, motif());
            else if (events & (TMZ_EVENT_TOILET_URGE | TMZ_EVENT_SICK | TMZ_EVENT_CRITICAL)) {
                tmz_audio_play(TMZ_SOUND_CALL, motif());
            }
            tmz_store_save_async(&s_model);
            if (s_current == SCREEN_HOME || s_current == SCREEN_STATUS) build_current();
        } else if (s_save_seconds >= 300) {
            s_save_seconds = 0;
            tmz_store_save_async(&s_model);
        }
        if (s_ui_seconds >= 10 && (s_current == SCREEN_HOME || s_current == SCREEN_STATUS)) {
            s_ui_seconds = 0;
            build_current();
        }
    }
    static uint32_t battery_seconds;
    battery_seconds += seconds;
    if (s_battery_available && battery_seconds >= 60) {
        battery_seconds = 0;
        s_battery = bsp_battery_soc();
    }
}

bool tmz_app_start(const tmz_model_t *initial_model, bool battery_available)
{
    if (!initial_model) return false;
    s_model = *initial_model;
    s_battery_available = battery_available;
    s_battery = battery_available ? bsp_battery_soc() : -1;
    s_key_queue = xQueueCreate(12, sizeof(key_message_t));
    if (!s_key_queue) return false;
    s_current = s_model.active ? SCREEN_HOME : SCREEN_SELECT;
    s_selection = 0;
    s_last_us = esp_timer_get_time();
    build_current();
    s_timer = lv_timer_create(timer_tick, 50, NULL);
    return s_timer != NULL;
}

void tmz_app_button(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (!s_key_queue) return;
    key_message_t key = { .button = button, .event = event };
    xQueueSend(s_key_queue, &key, 0);
}
