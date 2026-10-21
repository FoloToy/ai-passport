idf_component_register(
    SRCS "main.c"
         "fonts/lv_font_wb_zh_16.c"
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
         "demo_workbuddy.c"
         "demo_low_power.c"
         "workbuddy_companion.c"
    INCLUDE_DIRS "."
    REQUIRES bsp bt esp_event esp_hw_support esp_netif esp_timer esp_wifi lvgl nvs_flash wb_protocol
)
