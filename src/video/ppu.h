#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct pixel_processing_unit ppu_t;
typedef struct cpu cpu_t;
typedef struct display display_t;

ppu_t *ppu_new(mmu_t *mmu, cpu_t *interrupt_line, uint8_t *vram,
               display_t *display);

void ppu_delete(ppu_t *ppu);

bool ppu_update(ppu_t *ppu, uint8_t cycles);

