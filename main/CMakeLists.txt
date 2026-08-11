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
         "buddy_ble_store.c"
         "buddy_ble_lifecycle.c"
         "buddy_settings.c"
         "buddy_character.c"
         "buddy_ui.c"
    INCLUDE_DIRS "."
    REQUIRES bsp bt esp_hw_support esp_timer nvs_flash
)

# ESP-IDF 5.5.3 only applies WRITE_ENC to generated CCCDs. Wrap the private
# registration boundary so protected CCCDs also receive READ_ENC.
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=ble_att_svr_register")
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,-u,__wrap_ble_att_svr_register")
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,-u,buddy_ble_init")
