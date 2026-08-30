idf_component_register(
    SRCS "main.c"
         "passport_app.c"
         "passport_core.c"
         "passport_ui.c"
         "passport_net.c"
         "passport_store.c"
         "font_sm_cjk_16.c"
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
    INCLUDE_DIRS "."
    REQUIRES bsp bt console driver esp_app_format esp_event esp_hw_support
             esp_netif esp_timer esp_wifi nvs_flash esp_http_client mbedtls lwip
             qrcodegen jsmn
)
