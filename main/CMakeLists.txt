idf_component_register(
    SRCS "main.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
         "demo_display.c"
         "demo_button.c"
         "demo_audio.c"
         "demo_battery.c"
         "demo_tetris.c"
         "rtttl_player.c"
         "tetris_model.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_timer
)
