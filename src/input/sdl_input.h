#pragma once

#include "input_controller.h"
typedef struct mmu_t mmu_t;
typedef struct cpu cpu_t;

input_ctrl_t *sdl_joy_pad_new(mmu_t *mmu, cpu_t *interrupt_line);
