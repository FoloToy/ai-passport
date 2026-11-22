idf_component_register(
    SRCS "src/treaai.c"
         "src/treaai_protocol.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client json mbedtls nvs_flash
    PRIV_REQUIRES esp_timer
)
