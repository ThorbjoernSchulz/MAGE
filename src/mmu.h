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

uint8_t mmu_get_lcd_control_reg(mmu_t *mmu);

void mmu_inc_div_reg(mmu_t *mmu);

uint8_t mmu_get_div_reg(mmu_t *mmu);

uint8_t mmu_get_interrupt_enable_reg(mmu_t *mmu);

uint8_t mmu_get_interrupt_flags_reg(mmu_t *mmu);

void mmu_set_interrupt_enable_reg(mmu_t *mmu, uint8_t reg);

void mmu_set_interrupt_flags_reg(mmu_t *mmu, uint8_t reg);

void mmu_set_timer_counter_reg(mmu_t *mmu, uint8_t);

uint8_t mmu_get_timer_counter_reg(mmu_t *mmu);

uint8_t mmu_get_timer_modulo_reg(mmu_t *mmu);

uint8_t mmu_get_timer_control_reg(mmu_t *mmu);

void mmu_set_joypad_input(mmu_t *mmu, uint8_t value);

as_handle_t mmu_map_memory(mmu_t *mmu, uint16_t start, uint16_t end);

/* takes the descriptor for target and maps start to end to this descriptor */
void mmu_remap(mmu_t *mmu, uint16_t target, uint16_t start, uint16_t end);

void mmu_register_mem_handler(mmu_t *mmu, mem_handler_t *m, as_handle_t h);

void mmu_register_vram(mmu_t *mmu, mem_handler_t *handler);

uint8_t *mmu_get_oam_ram(mmu_t *mmu);

typedef struct lcd_display lcd_t;

lcd_t *lcd_new(mmu_t *mmu, uint8_t *vram, SDL_Surface *surface);

void lcd_delete(lcd_t *lcd);

void mmu_set_timer_clock(mmu_t *mmu, uint32_t clocks);

uint32_t mmu_get_timer_clock(mmu_t *mmu);
