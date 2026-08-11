idf_component_register(
    SRCS "main.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
         "demo_display.c"
         "demo_button.c"
         "demo_audio.c"
         "demo_battery.c"
         "buddy_line.c"
         "buddy_ble.c"
         "buddy_settings.c"
    INCLUDE_DIRS "."
    REQUIRES bsp bt esp_hw_support esp_timer nvs_flash
)
