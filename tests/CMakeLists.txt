cmake_minimum_required(VERSION 3.16)
project(buddy_host_tests C)

enable_testing()

add_executable(test_buddy_state test_buddy_state.c ../main/buddy_state.c)
target_include_directories(test_buddy_state PRIVATE ../main)
target_compile_options(test_buddy_state PRIVATE -Wall -Wextra -Werror)
add_test(NAME buddy_state COMMAND test_buddy_state)
