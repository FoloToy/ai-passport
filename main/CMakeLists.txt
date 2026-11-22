idf_component_register(
    SRCS "main.c"
         "treaai_ui.c"
         "treaai_app.c"
    INCLUDE_DIRS "."
    REQUIRES bsp treaai bt esp_event esp_hw_support esp_netif esp_timer esp_wifi nvs_flash
)
