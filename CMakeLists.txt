cmake_minimum_required(VERSION 3.13)
project(GameBoy C)

set(CMAKE_C_STANDARD 11)

set(CMAKE_C_COMPILER /usr/bin/clang)

include_directories(.)
include_directories(src)

set(PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

file(GLOB INTERNA_SRC ${PROJECT_SOURCE_DIR}/cpu/*.c
                      ${PROJECT_SOURCE_DIR}/input/*.c
                      ${PROJECT_SOURCE_DIR}/memory/*.c
                      ${PROJECT_SOURCE_DIR}/video/*.c
                      ${PROJECT_SOURCE_DIR}/cartridge.c
                      ${PROJECT_SOURCE_DIR}/logging.c)

add_library(Interna STATIC ${INTERNA_SRC} src/video/display.h src/video/sdl_display.c src/video/sdl_display.h src/input/input_strategy.h src/video/ascii_display.c src/video/ascii_display.h)

add_executable(GameBoy src/main.c src/gameboy.h src/gameboy.c)

target_link_libraries(GameBoy Interna SDL2)

add_subdirectory(tests)
