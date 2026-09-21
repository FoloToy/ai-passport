#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "game_i18n.h"

static unsigned count_newlines(const char *text)
{
    unsigned count = 0;
    for (; *text; ++text) {
        if (*text == '\n') ++count;
    }
    return count;
}

int main(void)
{
    for (unsigned language = 0; language < GAME_LANGUAGE_COUNT; ++language) {
        assert(game_i18n_language_valid((game_language_t)language));
        const char *unknown = game_i18n_get(
            (game_language_t)language, GAME_TEXT_UNKNOWN);
        for (unsigned text = 0; text < GAME_TEXT_COUNT; ++text) {
            const char *value = game_i18n_get(
                (game_language_t)language, (game_text_id_t)text);
            assert(value != NULL);
            assert(value[0] != '\0');
            if (text != GAME_TEXT_UNKNOWN) assert(strcmp(value, unknown) != 0);
        }
        assert(count_newlines(game_i18n_get(
            (game_language_t)language, GAME_TEXT_FOOTER)) == 1);
        assert(count_newlines(game_i18n_get(
            (game_language_t)language, GAME_TEXT_FOOTER_OK_BACK)) == 0);
    }

    assert(!game_i18n_language_valid(GAME_LANGUAGE_COUNT));
    assert(strcmp(game_i18n_get(GAME_LANGUAGE_COUNT, GAME_TEXT_APP_TITLE),
                  game_i18n_get(GAME_LANGUAGE_ZH_CN, GAME_TEXT_APP_TITLE)) == 0);
    assert(strcmp(game_i18n_get(GAME_LANGUAGE_ENGLISH, GAME_TEXT_COUNT),
                  game_i18n_get(GAME_LANGUAGE_ENGLISH, GAME_TEXT_UNKNOWN)) == 0);
    assert(strcmp(game_i18n_get(GAME_LANGUAGE_ZH_CN, GAME_TEXT_APP_TITLE),
                  game_i18n_get(GAME_LANGUAGE_ENGLISH, GAME_TEXT_APP_TITLE)) != 0);

    puts("game_i18n: all tests passed");
    return 0;
}
