#pragma once

#include "input_strategy.h"
typedef struct mmu_t mmu_t;
typedef struct cpu cpu_t;

input_strategy_t *sdl_joy_pad_new(void);
