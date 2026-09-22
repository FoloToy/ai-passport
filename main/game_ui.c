#include "game_ui.h"
#include "game_ui_layout.h"
#include "game_story.h"

#include "lvgl.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define COLOR_BACKGROUND 0x07090D
#define COLOR_PANEL 0x111722
#define COLOR_BORDER 0x263448
#define COLOR_TEXT 0xF2F5F8
#define COLOR_DIM 0x8290A3
#define COLOR_BLUE 0x39BFFF
#define COLOR_RED 0xFF4E57
#define COLOR_GREEN 0x42DB88

static lv_obj_t *s_screen;
static lv_obj_t *s_title;
static lv_obj_t *s_timer;
static lv_obj_t *s_battery;
static lv_obj_t *s_panel;
static lv_obj_t *s_body;
static lv_obj_t *s_footer;
static lv_obj_t *s_cover;
static lv_font_t s_cjk_font;
static game_language_t s_language = GAME_LANGUAGE_ZH_CN;

LV_FONT_DECLARE(game_cjk_16);
extern const lv_image_dsc_t game_cover_startup;

static void set_cover_visible(bool visible)
{
    lv_obj_t *const standard_objects[] = {
        s_title, s_timer, s_battery, s_panel, s_footer,
    };
    for (size_t index = 0; index < sizeof(standard_objects) / sizeof(standard_objects[0]);
         ++index) {
        if (!standard_objects[index]) continue;
        if (visible) lv_obj_add_flag(standard_objects[index], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(standard_objects[index], LV_OBJ_FLAG_HIDDEN);
    }
    if (!s_cover) return;
    if (visible) lv_obj_remove_flag(s_cover, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_cover, LV_OBJ_FLAG_HIDDEN);
}

static const lv_font_t *font_for_language(game_language_t language)
{
    return language == GAME_LANGUAGE_ZH_CN
         ? &s_cjk_font
         : &lv_font_montserrat_14;
}

static const char *scene_name(game_scene_t scene, game_language_t language)
{
    static const game_text_id_t names[] = {
        GAME_TEXT_APP_TITLE,
        GAME_TEXT_APP_TITLE,
        GAME_TEXT_APP_TITLE,
        GAME_TEXT_ROOM_1,
        GAME_TEXT_LOCK_1,
        GAME_TEXT_ROOM_2,
        GAME_TEXT_LOCK_2,
        GAME_TEXT_FINAL_ROOM,
        GAME_TEXT_FINAL_QUESTION,
        GAME_TEXT_ENDING,
        GAME_TEXT_TIMEOUT,
    };
    game_text_id_t text = (unsigned)scene < sizeof(names) / sizeof(names[0])
                        ? names[scene]
                        : GAME_TEXT_UNKNOWN;
    return game_i18n_get(language, text);
}

static const char *view_text(game_view_t view, game_language_t language)
{
    game_text_id_t text;
    switch (view) {
        case GAME_VIEW_CLOCK:
            text = GAME_TEXT_VIEW_CLOCK;
            break;
        case GAME_VIEW_BOOK:
            text = GAME_TEXT_VIEW_BOOK;
            break;
        case GAME_VIEW_PHOTO:
            text = GAME_TEXT_VIEW_PHOTO;
            break;
        case GAME_VIEW_ROOM_1_PASSPORT:
            text = GAME_TEXT_VIEW_ROOM_1_PASSPORT;
            break;
        case GAME_VIEW_PASSPORT_LOG:
            text = GAME_TEXT_VIEW_PASSPORT_LOG;
            break;
        case GAME_VIEW_ROOM_2_PASSPORT:
            text = GAME_TEXT_VIEW_ROOM_2_PASSPORT;
            break;
        case GAME_VIEW_FINAL_ROOM:
            text = GAME_TEXT_VIEW_FINAL_ROOM;
            break;
        case GAME_VIEW_FINAL_DOOR:
            text = GAME_TEXT_VIEW_FINAL_DOOR;
            break;
        case GAME_VIEW_FINAL_PASSPORT:
            text = GAME_TEXT_VIEW_FINAL_PASSPORT;
            break;
        case GAME_VIEW_ROOT:
        default:
            return "";
    }
    return game_i18n_get(language, text);
}

static void append_items(char *buffer, size_t size, const char *const *items,
                         size_t count, size_t selected)
{
    size_t used = strlen(buffer);
    for (size_t i = 0; i < count && used < size; ++i) {
        int written = snprintf(buffer + used, size - used, "%c %s%s",
                               i == selected ? '>' : ' ', items[i],
                               i + 1U < count ? "\n" : "");
        if (written < 0) break;
        size_t added = (size_t)written;
        used += added < size - used ? added : size - used;
    }
}

static void append_menu(char *buffer, size_t size, const char *title,
                        const char *const *items, size_t count, size_t selected)
{
    size_t used = strlen(buffer);
    if (used < size) snprintf(buffer + used, size - used, "%s\n\n", title);
    append_items(buffer, size, items, count, selected);
}

static void append_notice(char *buffer, size_t size, const char *notice)
{
    size_t used = strlen(buffer);
    if (notice && notice[0] && used < size) {
        snprintf(buffer + used, size - used, "%s\n", notice);
    }
}

static void format_code(char *buffer, size_t size, const game_model_t *model)
{
    size_t used = (size_t)snprintf(buffer, size, "%s\n",
                                   scene_name(model->current_scene, model->language));
    for (uint8_t i = 0; i < model->code_length && used < size; ++i) {
        int written = snprintf(buffer + used, size - used, "%s%u%s",
                               i == model->code_cursor ? "[" : " ",
                               model->code_digits[i],
                               i == model->code_cursor ? "]" : " ");
        if (written < 0) break;
        size_t added = (size_t)written;
        used += added < size - used ? added : size - used;
    }
    if (used < size) {
        int written = snprintf(buffer + used, size - used, "\n%s",
                 game_i18n_get(model->language, GAME_TEXT_CODE_HELP));
        if (written > 0) {
            size_t added = (size_t)written;
            used += added < size - used ? added : size - used;
        }
    }

    game_text_id_t notice = GAME_TEXT_UNKNOWN;
    if (model->feedback != GAME_FEEDBACK_NONE) {
        notice = game_story_feedback_text(model->feedback);
    } else if (model->current_scene == GAME_SCENE_PUZZLE_1 &&
               model->hint_level > 0) {
        notice = game_story_hint_text(model->hint_level);
    }
    if (notice != GAME_TEXT_UNKNOWN && used < size) {
        snprintf(buffer + used, size - used, "\n%s",
                 game_i18n_get(model->language, notice));
    }
}

static game_text_id_t ending_name_text(game_ending_t ending)
{
    switch (ending) {
        case GAME_ENDING_GUIDED:
            return GAME_TEXT_ENDING_GUIDED;
        case GAME_ENDING_GAME_OVER:
            return GAME_TEXT_ENDING_GAME_OVER;
        case GAME_ENDING_ESCAPED:
            return GAME_TEXT_ENDING_ESCAPED;
        case GAME_ENDING_TRUE_ESCAPE:
            return GAME_TEXT_ENDING_TRUE_ESCAPE;
        case GAME_ENDING_NONE:
        default:
            return GAME_TEXT_UNKNOWN;
    }
}

static void format_ending(char *buffer, size_t size, const game_model_t *model)
{
    const game_language_t language = model->language;
    const char *congratulations = game_story_ending_congratulates(model->ending)
                                ? game_i18n_get(language, GAME_TEXT_CONGRATULATIONS)
                                : NULL;
    snprintf(buffer, size, "%s: %s\n%s%s%s\n%s %02lu:%02lu\n%s",
             game_i18n_get(language, GAME_TEXT_ENDING),
             game_i18n_get(language, ending_name_text(model->ending)),
             congratulations ? congratulations : "",
             congratulations ? "\n" : "",
             game_i18n_get(language, game_story_ending_text(model->ending)),
             game_i18n_get(language, GAME_TEXT_TIME_LEFT),
             (unsigned long)(model->remaining_time / 60U),
             (unsigned long)(model->remaining_time % 60U),
             game_i18n_get(language, GAME_TEXT_THE_END));
}

static void format_body(char *buffer, size_t size, const game_model_t *model)
{
    const game_language_t language = model->language;
    const char *const intro_items[] = {
        game_i18n_get(language, GAME_TEXT_START_GAME),
        game_i18n_get(language, GAME_TEXT_SETTINGS),
    };
    const char *const language_items[] = {
        game_i18n_get(language, GAME_TEXT_LANGUAGE_ZH_CN),
        game_i18n_get(language, GAME_TEXT_LANGUAGE_ENGLISH),
    };
    const char *const room_1_items[] = {
        game_i18n_get(language, GAME_TEXT_CLOCK),
        game_i18n_get(language, GAME_TEXT_BOOK),
        game_i18n_get(language, GAME_TEXT_PHOTO),
        game_i18n_get(language, GAME_TEXT_DOOR),
        game_i18n_get(language, GAME_TEXT_PASSPORT),
    };
    const char *const room_2_items[] = {
        game_i18n_get(language, GAME_TEXT_BOX),
        game_i18n_get(language, GAME_TEXT_PASSPORT_LOG),
        game_i18n_get(language, GAME_TEXT_PASSPORT),
    };
    const char *const final_room_items[] = {
        game_i18n_get(language, GAME_TEXT_FINAL_ROOM),
        game_i18n_get(language, GAME_TEXT_LOCKED_DOOR),
        game_i18n_get(language, GAME_TEXT_PASSPORT),
    };
    const char *const final_choices[] = {
        game_i18n_get(language, GAME_TEXT_FINAL_CHOICE_A),
        game_i18n_get(language, GAME_TEXT_FINAL_CHOICE_B),
        game_i18n_get(language, GAME_TEXT_FINAL_CHOICE_C),
        game_i18n_get(language, GAME_TEXT_FINAL_CHOICE_D),
    };
    const char *const system_items[] = {
        game_i18n_get(language, GAME_TEXT_RESTART),
        game_i18n_get(language, GAME_TEXT_CONTINUE),
        game_i18n_get(language, GAME_TEXT_SETTINGS),
    };

    buffer[0] = '\0';
    if (model->settings_menu_open) {
        append_menu(buffer, size,
                    game_i18n_get(language, GAME_TEXT_LANGUAGE_TITLE),
                    language_items,
                    sizeof(language_items) / sizeof(language_items[0]),
                    model->settings_selection);
        return;
    }
    if (model->system_menu_open) {
        append_menu(buffer, size,
                    game_i18n_get(language, GAME_TEXT_SYSTEM_TITLE), system_items,
                    sizeof(system_items) / sizeof(system_items[0]),
                    model->system_menu_selection);
        return;
    }
    if (model->current_view != GAME_VIEW_ROOT) {
        snprintf(buffer, size, "%s", view_text(model->current_view, language));
        return;
    }
    switch (model->current_scene) {
        case GAME_SCENE_BOOT:
        case GAME_SCENE_COVER:
            snprintf(buffer, size, "%s\n\n%s",
                     game_i18n_get(language, GAME_TEXT_APP_TITLE),
                     game_i18n_get(language, GAME_TEXT_BOOT_STATUS));
            break;
        case GAME_SCENE_INTRO:
            snprintf(buffer, size, "%s\n\n%s\n\n",
                     game_i18n_get(language, GAME_TEXT_APP_TITLE),
                     game_i18n_get(language, GAME_TEXT_INTRO_TAGLINE));
            append_items(buffer, size, intro_items,
                         sizeof(intro_items) / sizeof(intro_items[0]),
                         model->intro_selection);
            break;
        case GAME_SCENE_ROOM_1:
            append_menu(buffer, size,
                        game_i18n_get(language, GAME_TEXT_ROOM_1), room_1_items,
                        sizeof(room_1_items) / sizeof(room_1_items[0]),
                        model->room_selection);
            break;
        case GAME_SCENE_PUZZLE_1:
        case GAME_SCENE_PUZZLE_2:
            format_code(buffer, size, model);
            break;
        case GAME_SCENE_ROOM_2:
            if (model->feedback == GAME_FEEDBACK_CORRECT) {
                append_notice(buffer, size,
                              game_i18n_get(language,
                                            game_story_feedback_text(model->feedback)));
            }
            append_menu(buffer, size,
                        game_i18n_get(language, GAME_TEXT_ROOM_2), room_2_items,
                        sizeof(room_2_items) / sizeof(room_2_items[0]),
                        model->room_selection);
            break;
        case GAME_SCENE_FINAL_ROOM:
            if (model->feedback == GAME_FEEDBACK_CORRECT) {
                append_notice(buffer, size,
                              game_i18n_get(language,
                                            game_story_feedback_text(model->feedback)));
            }
            append_menu(buffer, size,
                        game_i18n_get(language, GAME_TEXT_FINAL_ROOM), final_room_items,
                        sizeof(final_room_items) / sizeof(final_room_items[0]),
                        model->room_selection);
            break;
        case GAME_SCENE_FINAL_CHOICE:
            append_menu(buffer, size,
                        game_i18n_get(language, GAME_TEXT_FINAL_PROMPT), final_choices,
                        sizeof(final_choices) / sizeof(final_choices[0]),
                        model->final_choice_selection);
            break;
        case GAME_SCENE_ENDING:
            format_ending(buffer, size, model);
            break;
        case GAME_SCENE_TIMEOUT:
            snprintf(buffer, size, "%s",
                     game_i18n_get(language, GAME_TEXT_TIMEOUT_RESTART));
            break;
    }
}

void game_ui_create(void)
{
    s_cjk_font = game_cjk_16;
    s_cjk_font.fallback = &lv_font_montserrat_14;

    s_screen = lv_obj_create(NULL);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(COLOR_BACKGROUND), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);

    s_title = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_title, &s_cjk_font, 0);
    lv_obj_set_style_text_color(s_title, lv_color_hex(COLOR_BLUE), 0);
    lv_obj_set_pos(s_title, 12, 10);

    s_battery = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_battery, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_battery, lv_color_hex(COLOR_DIM), 0);
    lv_obj_align(s_battery, LV_ALIGN_TOP_RIGHT, -12, 10);

    s_timer = lv_label_create(s_screen);
    lv_obj_set_style_text_font(s_timer, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_timer, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_align(s_timer, LV_ALIGN_TOP_MID, 0, 38);

    s_panel = lv_obj_create(s_screen);
    lv_obj_set_pos(s_panel, 10, 72);
    lv_obj_set_size(s_panel, 220, 196);
    lv_obj_remove_flag(s_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_panel, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(s_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_panel, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_border_width(s_panel, 1, 0);
    lv_obj_set_style_radius(s_panel, 8, 0);
    lv_obj_set_style_pad_all(s_panel, 12, 0);

    s_body = lv_label_create(s_panel);
    lv_obj_set_size(s_body, 194, 170);
    lv_label_set_long_mode(s_body, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_body, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_body, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_style_text_line_space(s_body, 4, 0);
    lv_obj_center(s_body);

    s_footer = lv_label_create(s_screen);
    lv_obj_set_pos(s_footer, GAME_UI_FOOTER_X, GAME_UI_FOOTER_Y);
    lv_obj_set_size(s_footer, GAME_UI_FOOTER_WIDTH, GAME_UI_FOOTER_HEIGHT);
    lv_label_set_long_mode(s_footer, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_footer, &s_cjk_font, 0);
    lv_obj_set_style_text_color(s_footer, lv_color_hex(COLOR_DIM), 0);
    lv_obj_set_style_text_align(s_footer, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(s_footer, 2, 0);

    s_cover = lv_image_create(s_screen);
    lv_image_set_src(s_cover, &game_cover_startup);
    lv_obj_set_pos(s_cover, 0, 0);
    lv_obj_add_flag(s_cover, LV_OBJ_FLAG_HIDDEN);

    lv_screen_load(s_screen);
}

void game_ui_render(const game_model_t *model, int battery_soc)
{
    if (!model || !s_screen) return;
    if (model->current_scene == GAME_SCENE_COVER) {
        set_cover_visible(true);
        return;
    }
    set_cover_visible(false);
    s_language = model->language;
    const lv_font_t *font = font_for_language(s_language);
    char body[512];
    format_body(body, sizeof(body), model);
    lv_obj_set_style_text_font(s_title, font, 0);
    lv_label_set_text(s_title, game_i18n_get(s_language, GAME_TEXT_APP_TITLE));
    lv_obj_set_style_text_font(s_body, font, 0);
    lv_label_set_text(s_body, body);
    lv_obj_set_style_text_font(s_footer, font, 0);
    game_text_id_t footer_text = game_model_ok_returns_from_view(model)
                               ? GAME_TEXT_FOOTER_OK_BACK
                               : GAME_TEXT_FOOTER;
    lv_label_set_text(s_footer, game_i18n_get(s_language, footer_text));
    lv_label_set_text_fmt(s_timer, "%02lu:%02lu",
                          (unsigned long)(model->remaining_time / 60U),
                          (unsigned long)(model->remaining_time % 60U));
    if (battery_soc >= 0) lv_label_set_text_fmt(s_battery, "%d%%", battery_soc);
    else lv_label_set_text(s_battery, "--%");

    uint32_t color = COLOR_TEXT;
    if ((model->feedback != GAME_FEEDBACK_NONE &&
         model->feedback != GAME_FEEDBACK_CORRECT) ||
        model->current_scene == GAME_SCENE_TIMEOUT ||
        model->ending == GAME_ENDING_GAME_OVER) {
        color = COLOR_RED;
    } else if (model->feedback == GAME_FEEDBACK_CORRECT) {
        color = COLOR_BLUE;
    } else if (model->ending == GAME_ENDING_ESCAPED ||
               model->ending == GAME_ENDING_TRUE_ESCAPE) {
        color = COLOR_GREEN;
    }
    lv_obj_set_style_text_color(s_body, lv_color_hex(color), 0);
}

void game_ui_show_error(const char *message)
{
    if (!s_body) return;
    set_cover_visible(false);
    lv_obj_set_style_text_font(s_body, font_for_language(s_language), 0);
    lv_label_set_text_fmt(s_body, "%s\n\n%s",
                          game_i18n_get(s_language, GAME_TEXT_SYSTEM_ERROR),
                          message ? message
                                  : game_i18n_get(s_language, GAME_TEXT_UNKNOWN));
    lv_obj_set_style_text_color(s_body, lv_color_hex(COLOR_RED), 0);
}
