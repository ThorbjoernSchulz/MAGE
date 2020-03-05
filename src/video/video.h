#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef uint16_t gb_address_t;
typedef struct cpu cpu_t;

typedef struct {
  uint8_t lcd_control;
  uint8_t lcd_status;
  uint8_t scroll_y;
  uint8_t scroll_x;
  uint8_t lcdc_y;
  uint8_t ly_compare;
  uint8_t dma_transfer_and_start;
  uint8_t bg_palette;
  uint8_t obj_palette_0;
  uint8_t obj_palette_1;
  uint8_t window_y;
  uint8_t window_x;
} ppu_regs_t;

bool get_bg_data_select(ppu_regs_t *regs);

gb_address_t get_bg_data_start_offset(ppu_regs_t *regs);

bool get_bg_display_select(ppu_regs_t *regs);

gb_address_t get_bg_display_start_offset(ppu_regs_t *regs);

gb_address_t get_window_display_start_offset(ppu_regs_t *regs);

gb_address_t get_sprite_data_start_offset(ppu_regs_t *regs);

bool obj_height_is_16_bit(ppu_regs_t *regs);

bool window_enabled(ppu_regs_t *regs);

bool lcd_enabled(ppu_regs_t *regs);

bool obj_enabled(ppu_regs_t *regs);

void set_mode(cpu_t *interrupt_line, ppu_regs_t *regs, uint8_t mode);

int get_mode(ppu_regs_t *regs);

bool check_coincidence(ppu_regs_t *regs);
