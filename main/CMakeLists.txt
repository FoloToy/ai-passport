idf_component_register(
    SRCS "main.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
         "tamagezi_app.c"
         "tamagezi_audio.c"
         "tamagezi_model.c"
         "tamagezi_pet_view.c"
         "tamagezi_store.c"
    INCLUDE_DIRS "."
    REQUIRES bsp nvs_flash esp_timer
)
