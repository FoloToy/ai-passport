cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Community firmware replaces the bootloader when installed over USB. Keep the
# factory Recovery entry gesture in every derivative build so USB development
# does not strand the device outside the mini-program BLE install path.
idf_build_set_property(
    BOOTLOADER_EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/bootloader_components/"
    APPEND)

project(FoloToy-AI-Passport)
