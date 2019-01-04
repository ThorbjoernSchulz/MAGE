cmake_minimum_required(VERSION 3.13)
project(GameBoy C)

set(CMAKE_C_STANDARD 11)

set(CMAKE_C_COMPILER /usr/bin/clang)

include_directories(.)

add_library(Interna STATIC
        src/execute.c
        src/cpu.h src/cpu.c
        src/instructions.h src/instructions.c
        src/timer.c
        src/lcd.c
        src/mmu.h src/mmu.c
        src/interrupts.h src/interrupts.c
        src/memory_handler.h src/mem_handler.c
        src/video.h src/video.c
        src/input.c
        src/cartridge.h src/cartridge.c)

add_executable(GameBoy src/main.c src/gameboy.h src/gameboy.c)
target_link_libraries(GameBoy Interna SDL2)

add_subdirectory(tests)
