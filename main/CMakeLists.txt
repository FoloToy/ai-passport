idf_component_register(
    SRCS "main.c"
         "ui_pixel.c"
         "safety_profile.c"
         "safety_store.c"
         "safety_portal.c"
         "ui_safety.c"
         "jpeg_probe.c"
         "jpeg_store.c"
         "jpeg_view.c"
         "lv_font_cn_16.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_event esp_http_server esp_hw_support esp_netif esp_partition
             esp_timer esp_wifi json lwip mbedtls nvs_flash
    PRIV_REQUIRES esp_new_jpeg lvgl
)
