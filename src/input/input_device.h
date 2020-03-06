#pragma once

#include <stdint.h>

#define GAME_BOY_START  0x80
#define GAME_BOY_SELECT 0x40
#define GAME_BOY_B      0x20
#define GAME_BOY_A      0x10
#define GAME_BOY_DOWN   0x8
#define GAME_BOY_UP     0x4
#define GAME_BOY_LEFT   0x2
#define GAME_BOY_RIGHT  0x1

typedef struct cpu cpu_t;
typedef struct mmu_t mmu_t;
typedef struct input_device input_device_t;

input_device_t *input_new(cpu_t *interrupt_line , mmu_t *mmu);
void input_delete(input_device_t *input);

void input_press(input_device_t *input, uint8_t key);
void input_release(input_device_t *input, uint8_t key);

