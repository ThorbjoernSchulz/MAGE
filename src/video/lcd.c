#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <SDL2/SDL_surface.h>
#include "src/cpu/interrupts.h"
#include "src/memory/mmu.h"
#include "src/cpu/cpu.h"
#include "src/memory/memory_handler.h"
#include "src/logging.h"

#include "video.h"
#include "iterator.h"

typedef struct oam_entry {
  uint8_t pos_y;
  uint8_t pos_x;
  uint8_t tile_number;
  uint8_t flags;
} oam_entry_t;

typedef struct lcd_display {
  uint8_t *vram;
  oam_entry_t *oam;
  lcd_regs_t *registers;
  SDL_Surface *display;
  camera_iterator_t beam_position;
  uint16_t scan_line_counter;
} lcd_t;

static void reset_beam(lcd_t *lcd);

static void decode_palette(uint8_t reg, uint32_t *const palette);

static void draw_sprites(lcd_t *lcd, uint32_t *screen_buffer);

static int sprite_cmp(const void *_a, const void *_b);

static void
draw_sprite(lcd_t *lcd, uint32_t *screen_buffer, oam_entry_t *sprite,
            uint32_t *palette0, uint32_t *palette1);

void *mmu_get_lcd_regs(mmu_t *mmu);

lcd_t *lcd_new(mmu_t *mmu, uint8_t *vram, SDL_Surface *surface) {
  lcd_t *lcd = calloc(1, sizeof(lcd_t));
  if (!lcd) {
    logging_std_error();
    return 0;
  }

  lcd->vram = vram;
  lcd->oam = (oam_entry_t *) mmu_get_oam_ram(mmu);
  lcd->registers = mmu_get_lcd_regs(mmu);
  lcd->display = surface;
  lcd->scan_line_counter = 456;
  reset_beam(lcd);

  return lcd;
}

void lcd_delete(lcd_t *lcd) {
  free(lcd);
}

static const uint32_t to_sdl_color[] = {
    0x00FFFFFF, 0x00AAAAAA, 0x00555555, 0x00000000
};

static void decode_palette(uint8_t reg, uint32_t *const palette) {
  palette[0] = to_sdl_color[(reg >> 0) & 0x3];
  palette[1] = to_sdl_color[(reg >> 4) & 0x3];
  palette[2] = to_sdl_color[(reg >> 2) & 0x3];
  palette[3] = to_sdl_color[(reg >> 6) & 0x3];
}

static void render_line(lcd_t *lcd) {
  if (!lcd->display) return;

  camera_iterator_t *it = &lcd->beam_position;
  lcd_regs_t *regs = lcd->registers;
  int line_no = regs->lcdc_y;
  uint32_t *screen_buffer = lcd->display->pixels;

  update_scrolling(it, regs);

  /* pick the right strategy to find the tiles in memory */
  find_tile_t find_tile =
      get_bg_data_select(regs) ? find_tile_unsigned : find_tile_signed;

  uint32_t palette[4];
  decode_palette(regs->bg_palette, palette);

  /* compute start and end of the line and then draw it */
  int buffer_index = line_no * 160;
  int end_index = buffer_index + 160;
  while (buffer_index != end_index)
    screen_buffer[buffer_index++] = next_pixel(it, find_tile, palette);
}

static void draw_sprites(lcd_t *lcd, uint32_t *screen_buffer) {
  oam_entry_t *sprites[40] = {0};
  size_t active_sprites = 0;

  /* which sprites are visible at all? */
  for (int i = 0; i < 40; ++i) {
    if (lcd->oam[i].pos_x == 0 || lcd->oam[i].pos_x >= 168 ||
        lcd->oam[i].pos_y == 0 || lcd->oam[i].pos_y >= 160)
      continue;

    sprites[active_sprites++] = &lcd->oam[i];
  }

  /* sort sprites by priority */
  qsort((void *) sprites, active_sprites, sizeof(oam_entry_t *), sprite_cmp);

  lcd_regs_t *regs = lcd->registers;

  uint32_t palette_0[4] = {0};
  decode_palette(regs->obj_palette_0, palette_0);

  uint32_t palette_1[4] = {0};
  decode_palette(regs->obj_palette_1, palette_1);

  for (int i = 0; i < active_sprites; ++i)
    /* draw sprite */
    draw_sprite(lcd, screen_buffer, sprites[i], palette_0, palette_1);
}

static int sprite_cmp(const void *_a, const void *_b) {
  const oam_entry_t *a = (const oam_entry_t *) _a;
  const oam_entry_t *b = (const oam_entry_t *) _b;

  if (a->pos_x == b->pos_x) {
    /* 'a' has higher priority if it is first in the oam ram */
    return a < b ? -1 : 1;
  }

  return a->pos_x < b->pos_x ? -1 : 1;
}

static void
draw_sprite(lcd_t *lcd, uint32_t *screen_buffer, oam_entry_t *sprite,
            uint32_t *palette0, uint32_t *palette1) {
  lcd_regs_t *regs = lcd->registers;

  /* first figure out how the sprite should be drawn */
  uint32_t *palette = sprite->flags & 0x10 ? palette1 : palette0;

  uint32_t bg_low_prio_color = to_sdl_color[(regs->bg_palette >> 0) & 0x3];

  sprite_px_it_t it = {.tile = (px_data_t *) lcd->vram + sprite->tile_number};

  bool y_flip = (sprite->flags & 0x40) != 0;
  bool x_flip = (sprite->flags & 0x20) != 0;

  int sprite_height = obj_height_is_16_bit(regs) ? 16 : 8;

  if (y_flip) /* y flip */ {
    it.y = sprite_height - 1;
  }

  /* ok, let's draw it to the screen */
  int screen_y = (int) sprite->pos_y - 16;

  for (int y = 0; y < sprite_height; ++y, ++screen_y) {
    int screen_x = (int) sprite->pos_x - 8;

    /* is the pixel visible ? */
    if (screen_y < 0 || screen_y >= 144) continue;

    for (int x = 0; x < 8; ++x, ++screen_x) {
      int color = next_sprite_pixel(&it, x_flip, y_flip);
      if (!color) continue;

      if (screen_x < 0 || screen_x >= 160) continue;

      int pixel_pos = screen_y * 160 + screen_x;

      if (pixel_pos >= 160 * 144) continue;

      if (sprite->flags & 0x80) { /* priority bit */
        if (screen_buffer[pixel_pos] != bg_low_prio_color) continue;
      }

      screen_buffer[pixel_pos] = palette[color];
    }
  }
}

int mode_0_h_blank(lcd_t *lcd, cpu_t *bus) {
  lcd_regs_t *regs = lcd->registers;

  if (lcd->scan_line_counter >= 204) {
    lcd->scan_line_counter -= 204;
    set_mode(bus, regs, 2);

    render_line(lcd);

    if (++regs->lcdc_y == 144) {
      regs->lcd_status += 1;

      raise_interrupt(bus, INT_VBLANK);
      set_mode(bus, regs, 1);
    }
  }

  return 0;
}

int mode_1_v_blank(lcd_t *lcd, cpu_t *bus) {
  lcd_regs_t *regs = lcd->registers;

  if (lcd->scan_line_counter >= 456) {
    lcd->scan_line_counter -= 456;

    if (++regs->lcdc_y == 154) {
      regs->lcdc_y = 0;
      set_mode(bus, regs, 2);
      reset_beam(lcd);
      return 1;
    }
  }

  return 0;
}

static void reset_beam(lcd_t *lcd) {
  lcd->beam_position = reset_iterator(lcd->registers, lcd->vram);
}

int mode_2_search_oam(lcd_t *lcd, cpu_t *bus) {
  lcd_regs_t *regs = lcd->registers;

  if (lcd->scan_line_counter >= 80) {
    lcd->scan_line_counter -= 80;
    set_mode(bus, regs, 3);
  }

  return 0;
}

int mode_3_transfer_data(lcd_t *lcd, cpu_t *bus) {
  lcd_regs_t *regs = lcd->registers;

  if (lcd->scan_line_counter >= 172) {
    lcd->scan_line_counter -= 172;

    if (check_coincidence(regs)) {
      raise_interrupt(bus, INT_LCD_STAT);
    }

    set_mode(bus, regs, 0);
  }

  return 0;
}

int (*const run_mode[])(lcd_t *lcd, cpu_t *bus) = {
    mode_0_h_blank, mode_1_v_blank, mode_2_search_oam, mode_3_transfer_data
};

typedef struct cpu cpu_t;
/*
 * Updates the LCD inner state. Returns true if the screen should be updated.
 * .cpu         The game boys cpu.
 * .cycles      The amount of cpu cycles passed since the last update.
 */
bool run_lcd(cpu_t *cpu, uint8_t cycles) {
  lcd_t *lcd = cpu->lcd;
  lcd_regs_t *regs = lcd->registers;
  bool update_screen = false;

  if (!lcd_enabled(regs)) {
    lcd->scan_line_counter = 456;
    regs->lcdc_y = 0x99;

    set_mode(cpu, regs, 1);
    return false;
  }

  lcd->scan_line_counter += cycles;
  if (run_mode[get_mode(regs)](lcd, cpu)) {
    if (obj_enabled(lcd->registers))
      draw_sprites(lcd, lcd->display->pixels);

    update_screen = true;
  }

  return update_screen;
}

