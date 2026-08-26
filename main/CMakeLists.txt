idf_component_register(
    SRCS "main.c"
         "font_muyu_22.c"
    INCLUDE_DIRS "."
    REQUIRES bsp
    EMBED_FILES "assets/coloros_muyu_bg.rgb565"
)
