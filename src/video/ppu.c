#include <SDL2/SDL_surface.h>

#include <logging.h>
#include <memory/memory_handler.h>
#include <memory/mmu.h>
#include <cpu/interrupts.h>
#include "ppu.h"
#include "video.h"
#include "iterator.h"
#include "display.h"

#define REG_BASE 0xFF40
#define OAM_BASE 0xFE00

static void decode_palette(uint8_t reg, uint8_t *const palette) {
  palette[0] = (reg >> 0) & 0x3;
  palette[1] = (reg >> 4) & 0x3;
  palette[2] = (reg >> 2) & 0x3;
  palette[3] = (reg >> 6) & 0x3;
}

typedef struct oam_entry {
  uint8_t pos_y;
  uint8_t pos_x;
  uint8_t tile_number;
  uint8_t flags;
} oam_entry_t;

typedef struct ppu_memory_handler {
  mem_handler_t base;
  ppu_t *ppu;
} ppu_mem_handler_t;

typedef struct vram_handler {
  mem_handler_t base;
  uint8_t *video_ram;
} vram_handler_t;

typedef struct pixel_processing_unit {
  uint8_t *vram;
  oam_entry_t *oam;
  ppu_regs_t *registers;
  display_t *display;

  mmu_t *mmu; /* for DMA transfer */
  cpu_t *interrupt_line;

  oam_entry_t *visible_sprites[10];
  int num_visible_sprites;

  camera_iterator_t beam_position;
  uint16_t scan_line_counter;

  ppu_mem_handler_t handler;
  vram_handler_t vram_handler;
} ppu_t;

DEF_MEM_READ(ppu_read) {
  ppu_mem_handler_t *handler = (ppu_mem_handler_t *) this;
  ppu_regs_t *registers = handler->ppu->registers;

  if (address < 0xFEA0) {
    /* OAM memory */
    if (get_mode(registers) > 1)
      return 0xFF;

    return ((uint8_t *) handler->ppu->oam)[address - OAM_BASE];
  }

  uint8_t *start = (uint8_t *) registers;
  uint8_t byte = start[address - REG_BASE];

  switch (address) {
    case 0xFF41:
      byte |= 0x80;
      break;

    default:
      break;
  }

  return byte;
}

DEF_MEM_WRITE(ppu_write) {
  ppu_mem_handler_t *handler = (ppu_mem_handler_t *) this;
  ppu_regs_t *registers = handler->ppu->registers;

  if (address < 0xFEA0) {
    /* OAM memory */
    ((uint8_t *) handler->ppu->oam)[address - OAM_BASE] = value;
    return;
  }
  uint8_t *start = (uint8_t *) registers;

  uint8_t current_value = start[address - REG_BASE];

  switch (address) {
    case 0xFF41:
      /* the last three bits are read only */
      value = (value & ~7) | (current_value & 7);
      break;

    case 0xFF44:
      /* write to LCY */
      value = 0;
      break;

    case 0xFF46: {
      /* DMA transfer to OAM */
      gb_address_t source = value << 8;
      mmu_dma_transfer(handler->ppu->mmu, source, OAM_BASE);
      return;
    }

    default:
      break;
  }
  start[address - 0xFF40] = value;
}

static DEF_MEM_WRITE(vram_write) {
  vram_handler_t *handler = (vram_handler_t *) this;
  handler->video_ram[address & ~(0xE000)] = value;
}

static DEF_MEM_READ(vram_read) {
  vram_handler_t *handler = (vram_handler_t *) this;
  return handler->video_ram[address & ~(0xE000)];
}

static void reset_beam(ppu_t *ppu) {
  ppu->beam_position = reset_iterator(ppu->registers, ppu->vram);
}

ppu_t *ppu_new(mmu_t *mmu, cpu_t *interrupt_line, uint8_t *vram,
               display_t *display) {
  ppu_t *ppu = calloc(1, sizeof(ppu_t));
  if (!ppu) {
    logging_std_error();
    return 0;
  }
  ppu->mmu = mmu;
  ppu->interrupt_line = interrupt_line;

  ppu->handler.base.read = ppu_read;
  ppu->handler.base.write = ppu_write;
  ppu->handler.base.destroy = mem_handler_stack_destroy;
  ppu->handler.ppu = ppu;


  mem_tuple_t tuple = mmu_map_memory(mmu, OAM_BASE, OAM_BASE + 160);
  ppu->oam = (oam_entry_t *) tuple.memory;
  mmu_register_mem_handler(mmu, (mem_handler_t *) &ppu->handler, tuple.handle);

  tuple = mmu_map_memory(mmu, REG_BASE, REG_BASE + sizeof(ppu_regs_t) - 1);
  ppu->registers = (ppu_regs_t *) tuple.memory;
  mmu_register_mem_handler(mmu, (mem_handler_t *) &ppu->handler, tuple.handle);

  ppu->vram_handler.video_ram = vram;
  ppu->vram_handler.base.write = vram_write;
  ppu->vram_handler.base.read = vram_read;
  ppu->vram_handler.base.destroy = mem_handler_stack_destroy;

  mmu_assign_vram_handler(mmu, (mem_handler_t *)&ppu->vram_handler);

  ppu->vram = vram;
  ppu->display = display;
  ppu->scan_line_counter = 456;
  reset_beam(ppu);

  return ppu;
}

void ppu_delete(ppu_t *ppu) {
  free(ppu);
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

static void find_visible_sprites(ppu_t *ppu) {
  /* which sprites are visible at all? */
  ppu->num_visible_sprites = 0;
  ppu_regs_t *regs = ppu->registers;

  int ly = regs->lcdc_y + 16;
  int h = obj_height_is_16_bit(regs) ? 16 : 8;

  for (int i = 0; i < 40; ++i) {
    oam_entry_t *oam = &ppu->oam[i];
    if (oam->pos_x == 0 || ly < oam->pos_y || ly >= oam->pos_y + h)
      continue;

    ppu->visible_sprites[ppu->num_visible_sprites] = oam;
    if (++ppu->num_visible_sprites == 10)
      break;
  }

  /* sort sprites by priority */
  qsort((void *) ppu->visible_sprites, ppu->num_visible_sprites,
        sizeof(oam_entry_t *), sprite_cmp);
}

static void render_background_line(ppu_t *ppu, uint8_t *buffer) {
  if (!ppu->display) return;

  camera_iterator_t *it = &ppu->beam_position;
  ppu_regs_t *regs = ppu->registers;

  update_scrolling(it, regs);

  /* pick the right strategy to find the tiles in memory */
  find_tile_t find_tile =
      get_bg_data_select(regs) ? find_tile_unsigned : find_tile_signed;

  uint8_t palette[4];
  decode_palette(regs->bg_palette, palette);

  /* compute start and end of the line and then draw it */
  for (int i = 0; i < 160; ++i)
    buffer[i] = next_pixel(it, find_tile, palette);
}

static void render_sprite_line(ppu_t *ppu, uint8_t *buffer) {
  oam_entry_t **sprites = ppu->visible_sprites;
  int active_sprites = ppu->num_visible_sprites;

  ppu_regs_t *regs = ppu->registers;

  uint8_t palette_0[4] = {0};
  decode_palette(regs->obj_palette_0, palette_0);

  uint8_t palette_1[4] = {0};
  decode_palette(regs->obj_palette_1, palette_1);

  for (int i = active_sprites; i--;) {
    oam_entry_t *sprite = sprites[i];
    uint8_t *palette = sprite->flags & 0x10 ? palette_1 : palette_0;

    px_data_t *tile = ((px_data_t *) ppu->vram) + sprite->tile_number;

    bool y_flip = (sprite->flags & 0x40) != 0;
    bool x_flip = (sprite->flags & 0x20) != 0;

    uint8_t sprite_line = regs->lcdc_y + 16 - sprite->pos_y;
    if (y_flip) {
      uint8_t h = obj_height_is_16_bit(regs) ? 16 : 8;
      sprite_line = h - sprite_line - 1;
    }

    uint16_t line = decode_tile_line(tile, sprite_line);
    if (x_flip) { line = flip_line(line); }

    for (int j = 0; j < 8; ++j) {
      int idx = sprite->pos_x - 8 + j;
      if (idx < 0 | idx >= 160)
        continue;

      uint8_t color = get_pixel(line, j);
      if (!color)
        continue;

      /* color | visibility | priority-bit */
      buffer[idx] = palette[color] | (color << 2) | (sprite->flags & 0x80);
    }
  }
}

static void
combine_lines(uint8_t *background, uint8_t *sprites, uint8_t zero_color) {
  for (int i = 0; i < 160; ++i) {
    uint8_t sprite = sprites[i] & 3;
    uint8_t visible = (sprites[i] >> 2) & 3;
    bool priority = (sprites[i] & 0x80) != 0;

    if (!visible)
      continue;

    if (priority && (background[i] != zero_color))
      continue;

    background[i] = sprite;
  }
}

static void render_line(ppu_t *ppu) {
  uint8_t background[160];
  uint8_t sprites[160] = {0};

  render_background_line(ppu, background);

  if (obj_enabled(ppu->registers)) {
    render_sprite_line(ppu, sprites);
    combine_lines(background, sprites, ppu->registers->bg_palette & 3);
  }

  ppu->display->draw_line(ppu->display, background);
}

static void lcd_new_line(ppu_t *ppu) {
  /* switch to oam mode */
  find_visible_sprites(ppu);
  set_mode(ppu->interrupt_line, ppu->registers, 2);
}

int mode_0_h_blank(ppu_t *ppu) {
  ppu_regs_t *regs = ppu->registers;

  if (ppu->scan_line_counter >= 204) {
    ppu->scan_line_counter -= 204;

    lcd_new_line(ppu);
    render_line(ppu);

    if (++regs->lcdc_y == 144) {
      regs->lcd_status += 1;

      raise_interrupt(ppu->interrupt_line, INT_VBLANK);
      set_mode(ppu->interrupt_line, regs, 1);
    }
  }

  return 0;
}

int mode_1_v_blank(ppu_t *ppu) {
  ppu_regs_t *regs = ppu->registers;

  if (ppu->scan_line_counter >= 456) {
    ppu->scan_line_counter -= 456;

    if (++regs->lcdc_y == 154) {
      lcd_new_line(ppu);
      reset_beam(ppu);
      regs->lcdc_y = 0;
      return 1;
    }
  }

  return 0;
}

int mode_2_search_oam(ppu_t *ppu) {
  ppu_regs_t *regs = ppu->registers;

  if (ppu->scan_line_counter >= 80) {
    ppu->scan_line_counter -= 80;
    set_mode(ppu->interrupt_line, regs, 3);
  }

  return 0;
}

int mode_3_transfer_data(ppu_t *ppu) {
  ppu_regs_t *regs = ppu->registers;

  if (ppu->scan_line_counter >= 172) {
    ppu->scan_line_counter -= 172;

    if (check_coincidence(regs)) {
      raise_interrupt(ppu->interrupt_line, INT_LCD_STAT);
    }

    set_mode(ppu->interrupt_line, regs, 0);
  }

  return 0;
}

int (*const run_mode[])(ppu_t *ppu) = {
    mode_0_h_blank, mode_1_v_blank, mode_2_search_oam, mode_3_transfer_data
};

typedef struct cpu cpu_t;
/*
 * Updates the LCD inner state. Returns true if the screen should be updated.
 * .cycles      The amount of cpu cycles passed since the last update.
 */
bool ppu_update(ppu_t *ppu, uint8_t cycles) {
  ppu_regs_t *regs = ppu->registers;
  bool update_screen = false;

  if (!lcd_enabled(regs)) {
    ppu->scan_line_counter = 456;
    regs->lcdc_y = 0x99;

    set_mode(ppu->interrupt_line, regs, 1);
    return false;
  }

  ppu->scan_line_counter += cycles;
  if (run_mode[get_mode(regs)](ppu)) {
    update_screen = true;
  }

  return update_screen;
}
