cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(FoloToy-AI-Passport)

# LVGL 默认的断言处理器是 `while(1);`。一旦 LVGL 内部（例如它按半径缓存的圆形抗锯齿
# 蒙版）把 LVGL 内存池吃光，分配失败就会走到这个死循环：LVGL 任务原地空转、IDLE 被
# 饿死、界面永久卡死，而且除了一串看门狗告警之外没有任何可定位的信息。
# 改成 abort()：触发 panic 并打印回溯（CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT=y 会随后
# 重启），现场可定位、设备也能自己恢复。
# 处理器在 lv_conf_internal.h 里受 #ifndef 保护，所以编译期定义可以覆盖它；
# 它必须作用于 LVGL 组件自身的编译（断言在 LVGL 源码里），因此用 PUBLIC 定义。
# 分号必须写成 \; —— CMake 把未转义的 ; 当列表分隔符，会把本定义拆成两条，
# 宏只剩 abort() 而丢掉结尾分号，展开后变成 "abort() }" 而编译不过。
# abort() 的声明由 CONFIG_LV_ASSERT_HANDLER_INCLUDE 提供（见 sdkconfig.defaults）。
idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
if(lvgl_lib)
    target_compile_definitions(${lvgl_lib} PUBLIC "LV_ASSERT_HANDLER=abort()\;")
else()
    message(WARNING "lvgl component not found; LV_ASSERT_HANDLER stays while(1)")
endif()
