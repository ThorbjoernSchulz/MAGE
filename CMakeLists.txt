cmake_minimum_required(VERSION 3.13)
project(GameBoy C)

set(CMAKE_C_STANDARD 11)

set(CMAKE_C_COMPILER /usr/bin/clang)

include_directories(.)

add_library(Interna STATIC
        src/cpu/execute.c
        src/cpu/cpu.h src/cpu/cpu.c
        src/cpu/instructions.h src/cpu/instructions.c
        src/cpu/timer.c
        src/video/lcd.c
        src/memory/mmu.h src/memory/mmu.c
        src/cpu/interrupts.h src/cpu/interrupts.c
        src/memory/memory_handler.h src/memory/mem_handler.c
        src/video/video.h src/video/video.c
        src/input/input.c
        src/cartridge.h src/cartridge.c
        src/logging.c
        src/video/iterator.c
        src/input/input.h src/input/sdl_input.c src/input/sdl_input.h src/cpu/timer.h)

add_executable(GameBoy src/main.c src/gameboy.h src/gameboy.c)
target_link_libraries(GameBoy Interna SDL2 -fsanitize=address)

target_compile_options(GameBoy PRIVATE -fsanitize=address -fomit-frame-pointer -g)
target_compile_options(Interna PRIVATE -fsanitize=address -fomit-frame-pointer -g)

add_subdirectory(tests)
