#include "mmu.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define MMU_START_HANDLE 1

#include "memory_handler.h"

static uint8_t bootupcode [256];

void load_boot_rom(FILE *stream) {
  fread(bootupcode, 1, sizeof(bootupcode), stream);
}

static uint8_t __mmu_read(mmu_t *mmu, gb_address_t address);
static uint8_t mmu_boot_read(mmu_t *mmu, gb_address_t address);
uint8_t mmu_read(mmu_t *mmu, gb_address_t address);
void mmu_write(mmu_t *mmu, gb_address_t address, uint8_t value);

typedef struct mmu_handler {
  mem_handler_t base;
  mmu_t *mmu;
} mmu_handler_t;

typedef struct mmu_t {
  uint8_t internal_ram[8 * 1024];
  uint8_t high_memory[512];

  as_handle_t address_space[256];
  as_handle_t current_handle;

  mem_handler_t *memory_handlers[256];

  uint8_t *booting_done;
  uint8_t (*read)(mmu_t *, gb_address_t);

  mmu_handler_t internal_mem_handler;

  uint32_t timer_clock;
} mmu_t;

uint8_t get_joypad_state(bool direction);

void mmu_set_timer_clock(mmu_t *mmu, uint32_t clocks) {
  mmu->timer_clock = clocks;
}

uint32_t mmu_get_timer_clock(mmu_t *mmu) {
  return mmu->timer_clock;
}

static DEF_MEM_READ(internal_read) {
  mmu_t *mmu = ((mmu_handler_t *)this)->mmu;
  if (address >= 0xFE00) {
    uint8_t byte = mmu->high_memory[address - 0xFE00];

    if (address >= 0xFEA0 && address <= 0xFEFF)
      return 0;

    switch (address) {
      case 0xFF00: {
        /* reading joypad input depends on the bits written to this
         * address previously, specifically the 4th and 5th bit,
         * which decide whether a button or direction key was queried */
        if ((byte & 0x20) == 0) {
          return get_joypad_state(false);
        }
        if ((byte & 0x10) == 0) {
          return get_joypad_state(true);
        }
      }
      case 0xFF01:
        break;
      case 0xFF03: case 0xFF08: case 0xFF09: case 0xFF0A: case 0xFF0B:
      case 0xFF0C: case 0xFF0D: case 0xFF0E:
        byte = 0xFF;
        break;

      case 0xFF0F:
        //byte |= 0xE0;
        break;

      case 0xFF11: case 0xFF16: case 0xFF1A:
        /* only bit 6, 7 can be read */
        byte &= 0xC0;
        break;
      case 0xFF14: case 0xFF19: case 0xFF1E: case 0xFF23:
        /* only bit 6 can be read */
        byte &= 0x40;
        break;
      case 0xFF1C:
        /* ony bit 5, 6 can be read */
        byte &= 0x60;
        break;
      case 0xFF41:
        byte |= 0x80;
      default:
        break;
    }

    return byte;
  }

  if (address >= 0xE000) address -= 0x2000;
  return mmu->internal_ram[address - 0xC000];
}

static DEF_MEM_WRITE(internal_write) {
  mmu_t *mmu = ((mmu_handler_t *)this)->mmu;
  if (address >= 0xFE00) {
    if (address >= 0xFEA0 && address <= 0xFEFF)
      return;

    if (address == 0xFF04 || address == 0xFF44) {
      /* write to DIV or LCY */
      value = 0;
    }

    if (address == 0xFF07) {
      value &= 7;
      if ((mmu_read(mmu, 0xFF07) & 3) != (value & 3)) {
        /* reset counter */
        mmu_set_timer_counter_reg(mmu, mmu_get_timer_modulo_reg(mmu));
        mmu->timer_clock = 0;
      }
    }

    if (address == 0xFF00) {
      /* the lower 4 bits are read only */
      uint8_t current = mmu->high_memory[address - 0xFE00];
      current &= 0x0F;
      current |= value & 0xF0;
      value = current;
    }

    if (address == 0xFF46) {
      /* DMA transfer */
      gb_address_t source = value << 8;
      for (int i = 0; i < 0xA0; ++i) {
        uint8_t byte = mmu_read(mmu, (gb_address_t)(source + i));
        mmu_write(mmu, (gb_address_t)(0xFE00 + i), byte);
      }

      return;
    }

    mmu->high_memory[address - 0xFE00] = value;
    return;
  }

  if (address >= 0xE000) address -= 0x2000;
  mmu->internal_ram[address - 0xC000] = value;
}

static void mmu_handler_init(mmu_t *mmu) {
  mmu->internal_mem_handler.mmu = mmu;
  mmu->internal_mem_handler.base.read = internal_read;
  mmu->internal_mem_handler.base.write = internal_write;
  mmu->internal_mem_handler.base.destroy = mem_handler_stack_destroy;
}

static void mmu_do_map(mmu_t *mmu, uint16_t start, uint16_t end, as_handle_t h) {
  assert(start < end);
  start /= 256;
  if (end == 0xFFFF) { end /= 256; ++end; }
  else end /= 256;

  for (; start != end; ++start)
    mmu->address_space[start] = h;
}

as_handle_t mmu_map_memory(mmu_t *mmu, uint16_t start, uint16_t end) {
  assert(mmu->current_handle);
  mmu_do_map(mmu, start, end, mmu->current_handle);
  return mmu->current_handle++;
}

void mmu_remap(mmu_t *mmu, uint16_t target, uint16_t start, uint16_t end) {
  as_handle_t handle = mmu->address_space[target / 256];
  mmu_do_map(mmu, start, end, handle);
}

void mmu_register_mem_handler(mmu_t *mmu, mem_handler_t *m, as_handle_t h) {
  mmu->memory_handlers[h] = m;
}

mmu_t *mmu_new(void) {
  mmu_t *mmu = calloc(1, sizeof(mmu_t));
  if (!mmu) return 0;

  mmu->read = __mmu_read;
  mmu->booting_done = mmu->high_memory + 0x150;
  mmu->current_handle = MMU_START_HANDLE;

  /* setup internal memory handler */
  mmu_handler_init(mmu);

  as_handle_t idx = mmu_map_memory(mmu, 0xC000, 0xFFFF);
  mem_handler_t *handler = (mem_handler_t *)&mmu->internal_mem_handler;
  mmu_register_mem_handler(mmu, handler, idx);

  return mmu;
}

void mmu_delete(mmu_t *mmu) {
  while (--mmu->current_handle) {
    mem_handler_t *handler = mmu->memory_handlers[mmu->current_handle];
    handler->destroy(handler);
  }

  free(mmu);
}

static mem_handler_t *mmu_get_mem_handler(mmu_t *mmu, gb_address_t  address) {
  as_handle_t idx = mmu->address_space[address / 256];
  return mmu->memory_handlers[idx];
}

static uint8_t __mmu_read(mmu_t *mmu, gb_address_t address) {
  mem_handler_t *handler = mmu_get_mem_handler(mmu, address);
  return handler->read(handler, address);
}

static uint8_t mmu_boot_read(mmu_t *mmu, gb_address_t address) {
  if (*mmu->booting_done)
    /* disable boot */
    mmu->read = __mmu_read;

  if (address < 0x100) return bootupcode[address];

  return __mmu_read(mmu, address);
}

uint8_t mmu_read(mmu_t *mmu, gb_address_t address) {
  return mmu->read(mmu, address);
}

void mmu_write(mmu_t *mmu, gb_address_t address, uint8_t value) {
  mem_handler_t *handler = mmu_get_mem_handler(mmu, address);
  handler->write(handler, address, value);
}

void enable_boot_rom(mmu_t *mmu) {
  mmu->read = mmu_boot_read;
}

uint8_t mmu_get_interrupt_enable_reg(mmu_t *mmu) {
  return mmu_read(mmu, 0xFFFF);
}

void mmu_set_interrupt_enable_reg(mmu_t *mmu, uint8_t reg) {
  mmu_write(mmu, 0xFFFF, reg);
}

uint8_t mmu_get_interrupt_flags_reg(mmu_t *mmu) {
  return mmu_read(mmu, 0xFF0F);
}

void mmu_set_interrupt_flags_reg(mmu_t *mmu, uint8_t reg) {
  mmu_write(mmu, 0xFF0F, reg);
}

uint8_t mmu_get_lcd_control_reg(mmu_t *mmu) {
  return mmu_read(mmu, 0xFF40);
}

void mmu_inc_div_reg(mmu_t *mmu) {
  ++mmu->high_memory[0xFF04 - 0xFE00];
}

uint8_t mmu_get_div_reg(mmu_t *mmu) {
  return mmu_read(mmu, 0xFF04);
}

void mmu_set_timer_counter_reg(mmu_t *mmu, uint8_t value) {
  mmu_write(mmu, 0xFF05, value);
}

uint8_t mmu_get_timer_counter_reg(mmu_t *mmu) {
  return mmu_read(mmu, 0xFF05);
}

uint8_t mmu_get_timer_modulo_reg(mmu_t *mmu) {
  return mmu_read(mmu, 0xFF06);
}

uint8_t mmu_get_timer_control_reg(mmu_t *mmu) {
  return mmu_read(mmu, 0xFF07);
}

void *mmu_get_lcd_regs(mmu_t *mmu) {
  return (void*)(&mmu->high_memory[0x140]);
}

void mmu_clean(mmu_t *mmu) {
  memset(mmu->internal_ram, 0, sizeof(mmu->internal_ram));
  memset(mmu->high_memory, 0, sizeof(mmu->high_memory));
}

static void
mmu_direct_high_mem_write(mmu_t *mmu, gb_address_t address, uint8_t value) {
  mmu->high_memory[address - 0xFE00] = value;
}

void mmu_init_after_boot(mmu_t *mmu) {
  mmu_direct_high_mem_write(mmu, 0xFF05, 0x00);
  mmu_direct_high_mem_write(mmu, 0xFF06, 0x00);
  mmu_direct_high_mem_write(mmu, 0xFF07, 0x00);
  mmu_direct_high_mem_write(mmu, 0xFF10, 0x80);
  mmu_direct_high_mem_write(mmu, 0xFF11, 0xBF);
  mmu_direct_high_mem_write(mmu, 0xFF12, 0xF3);
  mmu_direct_high_mem_write(mmu, 0xFF14, 0xBF);
  mmu_direct_high_mem_write(mmu, 0xFF16, 0x3F);
  mmu_direct_high_mem_write(mmu, 0xFF17, 0x00);
  mmu_direct_high_mem_write(mmu, 0xFF19, 0xBF);
  mmu_direct_high_mem_write(mmu, 0xFF1A, 0x7F);
  mmu_direct_high_mem_write(mmu, 0xFF1B, 0xFF);
  mmu_direct_high_mem_write(mmu, 0xFF1C, 0x9F);
  mmu_direct_high_mem_write(mmu, 0xFF1E, 0xBF);
  mmu_direct_high_mem_write(mmu, 0xFF20, 0xFF);
  mmu_direct_high_mem_write(mmu, 0xFF21, 0x00);
  mmu_direct_high_mem_write(mmu, 0xFF22, 0x00);
  mmu_direct_high_mem_write(mmu, 0xFF23, 0xBF);
  mmu_direct_high_mem_write(mmu, 0xFF24, 0x77);
  mmu_direct_high_mem_write(mmu, 0xFF25, 0xF3);
  mmu_direct_high_mem_write(mmu, 0xFF26, 0xF1);
  mmu_direct_high_mem_write(mmu, 0xFF40, 0x91);
  mmu_direct_high_mem_write(mmu, 0xFF42, 0x00);
  mmu_direct_high_mem_write(mmu, 0xFF43, 0x00);
  mmu_direct_high_mem_write(mmu, 0xFF45, 0x00);
  mmu_direct_high_mem_write(mmu, 0xFF47, 0xFC);
  mmu_direct_high_mem_write(mmu, 0xFF48, 0xFF);
  mmu_direct_high_mem_write(mmu, 0xFF49, 0xFF);
  mmu_direct_high_mem_write(mmu, 0xFF4A, 0x00);
  mmu_direct_high_mem_write(mmu, 0xFF4B, 0x00);
  mmu_direct_high_mem_write(mmu, 0xFFFF, 0x00);

  mmu_direct_high_mem_write(mmu, 0xFF00, 0xFF);
  mmu_direct_high_mem_write(mmu, 0xFF41, 0x85);
  mmu_direct_high_mem_write(mmu, 0xFF44, 0x91);
}
void mmu_register_vram(mmu_t *mmu, mem_handler_t *handler) {
  as_handle_t idx = mmu_map_memory(mmu, 0x8000, 0xA000);
  mmu_register_mem_handler(mmu, handler, idx);
}

uint8_t *mmu_get_oam_ram(mmu_t *mmu) {
  return mmu->high_memory;
}

void mmu_set_joypad_input(mmu_t *mmu, uint8_t value) {
  mmu_direct_high_mem_write(mmu, 0xFF00, value);
}
