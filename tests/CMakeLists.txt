add_subdirectory(driver)

add_executable(GameBoyTests cpu_tests.c cb_instructions_test.c)
target_link_libraries(GameBoyTests TestDriver SDL2)

