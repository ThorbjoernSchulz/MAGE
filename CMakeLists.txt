cmake_minimum_required(VERSION 3.13)
project(GameBoy C)

set(CMAKE_C_STANDARD 11)

set(CMAKE_C_COMPILER /usr/bin/clang)

include_directories(.)
include_directories(src)

set(PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

file(GLOB INTERNA_SRC   ${PROJECT_SOURCE_DIR}/cpu/*.c
                        ${PROJECT_SOURCE_DIR}/input/*.c
                        ${PROJECT_SOURCE_DIR}/memory/*.c
                        ${PROJECT_SOURCE_DIR}/video/*.c
                        ${PROJECT_SOURCE_DIR}/cartridge.c
                        ${PROJECT_SOURCE_DIR}/logging.c)

file(GLOB INTERNA_HDRS  ${PROJECT_SOURCE_DIR}/cpu/*.h
                        ${PROJECT_SOURCE_DIR}/input/*.h
                        ${PROJECT_SOURCE_DIR}/memory/*.h
                        ${PROJECT_SOURCE_DIR}/video/*.h
                        ${PROJECT_SOURCE_DIR}/cartridge.h
                        ${PROJECT_SOURCE_DIR}/logging.h)

add_library(CtrlServer STATIC src/control_server/client.c
                              src/control_server/client.h)

add_library(Interna STATIC ${INTERNA_SRC} ${INTERNA_HDRS})

add_executable(GameBoy src/main.c src/gameboy.h src/gameboy.c)

target_link_libraries(GameBoy Interna CtrlServer SDL2)

add_subdirectory(tests)
