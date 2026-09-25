# Eggy Party application: build only the eggy_* sources + main.c + the ported
# RTTTL player. The baseline demo_*.c / ui_pixel*.c files stay on disk but are
# not compiled (the app boots into its own redesigned UI, not the test shell).
idf_component_register(
    SRCS "main.c"
         "eggy_rng.c"
         "eggy_run_model.c"
         "eggy_tap_model.c"
         "eggy_spin_model.c"
         "eggy_sfx.c"
         "rtttl_player.c"
         "eggy_ui.c"
         "eggy_hub.c"
         "eggy_run.c"
         "eggy_tap.c"
         "eggy_spin.c"
         "eggy_sleep.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_timer esp_hw_support lvgl
)
