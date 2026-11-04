idf_component_register(
    SRCS "main.c"
         "radio_stations.c"
         "radio_nvs.c"
         "radio_wifi.c"
         "radio_player.c"
         "radio_ui.c"
    INCLUDE_DIRS "."
    REQUIRES bsp nvs_flash esp_event esp_http_client esp_http_server esp_netif esp_wifi
             esp_timer esp_hw_support freertos
)

target_sources(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/radio_font_16.c"
)
