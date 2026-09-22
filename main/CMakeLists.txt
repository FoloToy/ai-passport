idf_component_register(
    SRCS "main.c"
         "game_i18n.c"
         "game_input.c"
         "game_model.c"
         "game_puzzles.c"
         "game_story.c"
         "game_timer.c"
         "game_ui.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_timer
)

target_sources(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/game_cjk_16.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/images/game_cover_startup.c"
)
