cmake_minimum_required(VERSION 3.16)
project(buddy_host_tests C)

enable_testing()

if(NOT DEFINED ENV{IDF_PATH} OR "$ENV{IDF_PATH}" STREQUAL "")
    message(FATAL_ERROR "IDF_PATH must point to an ESP-IDF checkout")
endif()

set(BUDDY_CJSON_DIR "$ENV{IDF_PATH}/components/json/cJSON")
set(BUDDY_CJSON_SOURCE "${BUDDY_CJSON_DIR}/cJSON.c")

add_executable(test_buddy_state test_buddy_state.c ../main/buddy_state.c
               ../main/buddy_protocol.c ${BUDDY_CJSON_SOURCE})
target_include_directories(test_buddy_state PRIVATE ../main ${BUDDY_CJSON_DIR})
target_compile_options(test_buddy_state PRIVATE -Wall -Wextra -Werror)
add_test(NAME buddy_state COMMAND test_buddy_state)

add_executable(test_buddy_line test_buddy_line.c ../main/buddy_line.c)
target_include_directories(test_buddy_line PRIVATE ../main)
target_compile_options(test_buddy_line PRIVATE -Wall -Wextra -Werror)
add_test(NAME buddy_line COMMAND test_buddy_line)

add_executable(test_buddy_ble test_buddy_ble.c ../main/buddy_ble.c ../main/buddy_ble_store.c
               ../main/buddy_ble_lifecycle.c)
target_include_directories(test_buddy_ble PRIVATE test_shims ../main)
target_compile_definitions(test_buddy_ble PRIVATE BUDDY_BLE_HOST_TEST)
target_compile_options(test_buddy_ble PRIVATE -Wall -Wextra -Werror)
add_test(NAME buddy_ble COMMAND test_buddy_ble)

add_executable(test_buddy_protocol test_buddy_protocol.c ../main/buddy_protocol.c
               ${BUDDY_CJSON_SOURCE})
target_include_directories(test_buddy_protocol PRIVATE ../main ${BUDDY_CJSON_DIR})
target_compile_options(test_buddy_protocol PRIVATE -Wall -Wextra -Werror)
add_test(NAME buddy_protocol COMMAND test_buddy_protocol)

add_executable(test_buddy_settings test_buddy_settings.c ../main/buddy_settings.c)
target_include_directories(test_buddy_settings PRIVATE test_shims ../main)
target_compile_definitions(test_buddy_settings PRIVATE BUDDY_SETTINGS_TESTING)
target_compile_options(test_buddy_settings PRIVATE -Wall -Wextra -Werror)
add_test(NAME buddy_settings COMMAND test_buddy_settings)

add_executable(test_buddy_character test_buddy_character.c ../main/buddy_character.c)
target_include_directories(test_buddy_character PRIVATE ../main)
target_compile_options(test_buddy_character PRIVATE -Wall -Wextra -Werror)
add_test(NAME buddy_character COMMAND test_buddy_character)

add_executable(test_buddy_app_logic test_buddy_app_logic.c ../main/buddy_app_logic.c)
target_include_directories(test_buddy_app_logic PRIVATE test_shims ../main)
target_compile_options(test_buddy_app_logic PRIVATE -Wall -Wextra -Werror)
add_test(NAME buddy_app_logic COMMAND test_buddy_app_logic)
