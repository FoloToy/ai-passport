#include <assert.h>
#include <stdio.h>

#include "funbox_model.h"

static void test_defaults_and_home_wrap(void)
{
    funbox_model_t model;
    funbox_model_init(&model, 123);
    assert(model.page == FUNBOX_PAGE_HOME);
    assert(funbox_model_volume(&model) == 75);
    assert(funbox_model_brightness(&model) == 100);
    assert(model.bgm_enabled);
    assert(funbox_model_apply(&model, FUNBOX_ACTION_PREV));
    assert(model.selected == FUNBOX_MENU_COUNT - 1);
    assert(funbox_model_apply(&model, FUNBOX_ACTION_NEXT));
    assert(model.selected == 0);
}
static void test_open_app_and_return(void)
{
    funbox_model_t model;
    funbox_model_init(&model, 456);
    assert(funbox_model_apply(&model, FUNBOX_ACTION_CONFIRM));
    assert(model.page == FUNBOX_PAGE_APP);
    assert(model.current_app == 0);
    assert(funbox_model_apply(&model, FUNBOX_ACTION_CONFIRM));
    assert(model.generation == 2);
    assert(funbox_model_apply(&model, FUNBOX_ACTION_BACK));
    assert(model.page == FUNBOX_PAGE_HOME);
}

static void test_name_options(void)
{
    funbox_model_t model;
    funbox_model_init(&model, 789);
    funbox_model_apply(&model, FUNBOX_ACTION_NEXT);
    funbox_model_apply(&model, FUNBOX_ACTION_CONFIRM);
    assert(model.current_app == 1);
    assert(funbox_model_apply(&model, FUNBOX_ACTION_PREV));
    assert(model.option == 2);
    assert(funbox_model_apply(&model, FUNBOX_ACTION_NEXT));
    assert(model.option == 0);
}

static void test_settings(void)
{
    funbox_model_t model;
    funbox_model_init(&model, 1011);
    model.selected = FUNBOX_PLAY_COUNT;
    funbox_model_apply(&model, FUNBOX_ACTION_CONFIRM);
    assert(model.page == FUNBOX_PAGE_SETTINGS);
    funbox_model_apply(&model, FUNBOX_ACTION_CONFIRM);
    assert(funbox_model_volume(&model) == 100);
    funbox_model_apply(&model, FUNBOX_ACTION_NEXT);
    funbox_model_apply(&model, FUNBOX_ACTION_CONFIRM);
    assert(!model.bgm_enabled);
    funbox_model_apply(&model, FUNBOX_ACTION_NEXT);
    funbox_model_apply(&model, FUNBOX_ACTION_CONFIRM);
    assert(funbox_model_brightness(&model) == 20);
}

static void test_global_love_shortcut(void)
{
    funbox_model_t model;
    funbox_model_init(&model, 1213);
    assert(funbox_model_apply(&model, FUNBOX_ACTION_LOVE));
    assert(model.page == FUNBOX_PAGE_LOVE);
    assert(model.current_app == 10);
}

static void test_random_limit(void)
{
    funbox_model_t model;
    funbox_model_init(&model, 1415);
    for (int i = 0; i < 100; i++) assert(funbox_model_random(&model, 7) < 7);
    assert(funbox_model_random(&model, 0) == 0);
}

int main(void)
{
    test_defaults_and_home_wrap();
    test_open_app_and_return();
    test_name_options();
    test_settings();
    test_global_love_shortcut();
    test_random_limit();
    puts("funbox_model: all tests passed");
    return 0;
}
