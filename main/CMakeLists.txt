idf_component_register(
    SRCS "main.c"
         "lucky_draw_logic.c"
         "app_lucky_draw.c"
         "net_wifi.c"
         "wifi_store.c"
         "wifi_prov.c"
         "prov_form.c"
         "ssq_result.c"
         "kl8_result.c"
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
    INCLUDE_DIRS "."
    REQUIRES bsp bt esp_event esp_hw_support esp_netif esp_timer esp_wifi nvs_flash
             esp_http_client esp-tls esp_http_server json mbedtls
)

# 快乐选号应用字体子集(见 assets/fonts/README 记录,lv_font_conv 生成)。
target_sources(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/lucky_font_16.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/lucky_font_20.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/lucky_font_24.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/lucky_font_40.c"
)
