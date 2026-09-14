idf_component_register(
    SRCS "main.c"
         "demo_navigation.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
         "demo_display.c"
         "demo_button.c"
         "demo_audio.c"
         "demo_battery.c"
         "demo_radio.c"
         "demo_wifi.c"
         "demo_ble.c"
         "demo_low_power.c"
         "demo_jinqiangua.c"
         "jinqiangua_logic.c"
    INCLUDE_DIRS "."
    REQUIRES bsp bt esp_event esp_hw_support esp_netif esp_timer esp_wifi nvs_flash
)
