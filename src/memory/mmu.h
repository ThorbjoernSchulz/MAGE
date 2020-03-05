#pragma once
typedef struct SDL_Surface SDL_Surface;

#include <stdint.h>

typedef struct mmu_t mmu_t;

typedef uint16_t gb_address_t;
typedef struct memory_handler mem_handler_t;
typedef uint8_t as_handle_t;

mmu_t *mmu_new(void);

void mmu_delete(mmu_t *mmu);

void mmu_clean(mmu_t *mmu);

typedef struct mem_tuple_t {
  uint8_t *memory;
  as_handle_t handle;
} mem_tuple_t;

mem_tuple_t mmu_map_memory(mmu_t *mmu, uint16_t start, uint16_t end);

mem_tuple_t mmu_map_register(mmu_t *mmu, gb_address_t addr);

void mmu_assign_rom_handler(mmu_t *mmu, mem_handler_t *handler);

void mmu_assign_wram_handler(mmu_t *mmu, mem_handler_t *handler);

void mmu_assign_vram_handler(mmu_t *mmu, mem_handler_t *handler);

void mmu_assign_extram_handler(mmu_t *mmu, mem_handler_t *handler);

void mmu_register_mem_handler(mmu_t *mmu, mem_handler_t *m, as_handle_t h);

void mmu_dma_transfer(mmu_t *mmu, gb_address_t from, gb_address_t to);

typedef struct pixel_processing_unit ppu_t;

typedef struct cpu cpu_t;
ppu_t *
ppu_new(mmu_t *mmu, cpu_t *interrupt_line, uint8_t *vram, SDL_Surface *surface);

void ppu_delete(ppu_t *ppu);

