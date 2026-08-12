idf_component_register(
    SRCS "main.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
         "demo_display.c"
         "demo_button.c"
         "demo_audio.c"
         "demo_battery.c"
         "demo_rps.c"
    INCLUDE_DIRS "."
    REQUIRES bsp
    EMBED_FILES "assets/rock.rgb565"
                "assets/scissors.rgb565"
                "assets/paper.rgb565"
)
