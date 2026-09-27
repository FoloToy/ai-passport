# Mini World application: build only the mini_* sources + rtttl_player + main.c.
# The baseline demo_*.c / ui_pixel*.c files stay on disk but are not compiled
# (the app boots into its own redesigned UI, not the test shell).
idf_component_register(
    SRCS "main.c"
         "mini_rng.c"
         "mini_world_model.c"
         "mini_sfx.c"
         "rtttl_player.c"
         "mini_ui.c"
         "mini_world.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_timer esp_hw_support lvgl
)
