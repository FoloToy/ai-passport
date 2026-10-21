idf_component_register(
    SRCS "src/wb_protocol.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer freertos nvs_flash
)
