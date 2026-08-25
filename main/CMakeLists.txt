# 自动收集宠物源文件（新增帧后需 idf.py reconfigure 让 CMake 重新扫描）
file(GLOB PET_SRCS "pet/*.c")

idf_component_register(
    SRCS "main.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
         "demo_display.c"
         "demo_button.c"
         "demo_audio.c"
         "demo_battery.c"
         "demo_pet.c"
         # —— 宠物（tools/png2lvgl.py → main/pet/pet_<action>_<frame>.{c,h}）——
         # GLOB 自动收集, 新增动作帧无需手改本文件; 加新文件后跑一次 idf.py reconfigure
         ${PET_SRCS}
    INCLUDE_DIRS "." "pet"
    REQUIRES bsp
)
