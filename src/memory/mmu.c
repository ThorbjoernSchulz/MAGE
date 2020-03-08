#include "mmu.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "memory_handler.h"

static uint8_t bootupcode[256];

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

#define MMU_MAX_HANDLE 32
#define MMU_START_HANDLE 1

struct __memory_handling {
  /* handling addresses from 0x0000 to 0x7FFF */
  mem_handler_t *ROM_handler;
  /* handling addresses from 0x8000 to 0x9FFF */
  mem_handler_t *VRAM_handler;
  /* handling addresses from 0xA000 to 0xBFFF */
  mem_handler_t *extRAM_handler;
  /* handling addresses from 0xC000 to 0xDFFF */
  mem_handler_t *WRAM_handler;

  mmu_handler_t echo_handler;

  /* maps concrete high memory addresses (>=0xFF00) to their memory handlers */
  as_handle_t high_mem_handles[512];
  as_handle_t current_handle;
  mem_handler_t *memory_handlers[MMU_MAX_HANDLE];
};

typedef struct memory_management_unit {
  uint8_t internal_ram[8 * 1024];
  uint8_t high_memory[512];

  struct __memory_handling address_space;

  uint8_t *booting_done;

  uint8_t (*read)(mmu_t *, gb_address_t);

  mmu_handler_t internal_mem_handler;
} mmu_t;

void mmu_assign_rom_handler(mmu_t *mmu, mem_handler_t *handler) {
  mmu->address_space.ROM_handler = handler;
}

void mmu_assign_wram_handler(mmu_t *mmu, mem_handler_t *handler) {
  mmu->address_space.WRAM_handler = handler;
}

void mmu_assign_vram_handler(mmu_t *mmu, mem_handler_t *handler) {
  mmu->address_space.VRAM_handler = handler;
}

void mmu_assign_extram_handler(mmu_t *mmu, mem_handler_t *handler) {
  mmu->address_space.extRAM_handler = handler;
}

static DEF_MEM_READ(echo_read) {
  mmu_t *mmu = ((mmu_handler_t *) this)->mmu;
  address -= 0x2000;
  return __mmu_read(mmu, address);
}

static DEF_MEM_WRITE(echo_write) {
  mmu_t *mmu = ((mmu_handler_t *) this)->mmu;
  address -= 0x2000;
  mmu_write(mmu, address, value);
}

static void __echo_handler_init(mmu_t *mmu) {
  mmu->address_space.echo_handler.mmu = mmu;
  mmu->address_space.echo_handler.base.read = echo_read;
  mmu->address_space.echo_handler.base.write = echo_write;
  mmu->address_space.echo_handler.base.destroy = mem_handler_stack_destroy;
}

static DEF_MEM_READ(internal_read) {
  mmu_t *mmu = ((mmu_handler_t *) this)->mmu;
  if (address >= 0xFE00) {
    uint8_t byte = mmu->high_memory[address - 0xFE00];

    if (address >= 0xFEA0 && address <= 0xFEFF)
      return 0;

    switch (address) {
      case 0xFF01:
        break;
      case 0xFF03:
      case 0xFF08:
      case 0xFF09:
      case 0xFF0A:
      case 0xFF0B:
      case 0xFF0C:
      case 0xFF0D:
      case 0xFF0E:
        byte = 0xFF;
        break;

      case 0xFF11:
      case 0xFF16:
      case 0xFF1A:
        /* only bit 6, 7 can be read */
        byte &= 0xC0;
        break;
      case 0xFF14:
      case 0xFF19:
      case 0xFF1E:
      case 0xFF23:
        /* only bit 6 can be read */
        byte &= 0x40;
        break;
      case 0xFF1C:
        /* ony bit 5, 6 can be read */
        byte &= 0x60;
        break;
      default:
        break;
    }

    return byte;
  }

  if (address >= 0xE000) address -= 0x2000;
  return mmu->internal_ram[address - 0xC000];
}

static DEF_MEM_WRITE(internal_write) {
  mmu_t *mmu = ((mmu_handler_t *) this)->mmu;
  if (address >= 0xFE00) {
    if (address >= 0xFEA0 && address <= 0xFEFF)
      return;

    if (address == 0xFF04) {
      /* write to DIV */
      value = 0;
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

static void
__mmu_do_map(mmu_t *mmu, uint16_t start, uint16_t end, as_handle_t h) {
  for (; start != end; ++start)
    mmu->address_space.high_mem_handles[start] = h;
}

mem_tuple_t mmu_map_register(mmu_t *mmu, gb_address_t addr) {
  return mmu_map_memory(mmu, addr, addr);
}

mem_tuple_t mmu_map_memory(mmu_t *mmu, uint16_t start, uint16_t end) {
  assert(start >= 0xFE00 && "Only high addresses can be mapped manually.");
  assert(start <= end);
  start -= 0xFE00;
  end -= 0xFE00;
  __mmu_do_map(mmu, start, end + 1, mmu->address_space.current_handle);
  mem_tuple_t ret = {mmu->high_memory + start,
                     mmu->address_space.current_handle++};
  return ret;
}

void mmu_register_mem_handler(mmu_t *mmu, mem_handler_t *m, as_handle_t h) {
  assert(h);
  mmu->address_space.memory_handlers[h] = m;
}

mmu_t *mmu_new(void) {
  mmu_t *mmu = calloc(1, sizeof(mmu_t));
  if (!mmu) return 0;

  mmu->read = __mmu_read;
  mmu->booting_done = mmu->high_memory + 0x150;
  mmu->address_space.current_handle = MMU_START_HANDLE;

  /* setup internal memory handler */
  mmu_handler_init(mmu);

  mem_handler_t *handler = (mem_handler_t *) &mmu->internal_mem_handler;
  mmu_assign_wram_handler(mmu, handler);

  mem_tuple_t tuple = mmu_map_memory(mmu, 0xFE00, 0xFFFF);
  mmu_register_mem_handler(mmu, handler, tuple.handle);

  __echo_handler_init(mmu);
  return mmu;
}

void mmu_delete(mmu_t *mmu) {
  mem_handler_t *handler = 0;

  as_handle_t i = mmu->address_space.current_handle;
  while (--i) {
    handler = mmu->address_space.memory_handlers[i];
    handler->destroy(handler);
  }

  handler = mmu->address_space.ROM_handler;
  handler->destroy(handler);

  handler = mmu->address_space.VRAM_handler;
  handler->destroy(handler);

  handler = mmu->address_space.extRAM_handler;
  handler->destroy(handler);

  handler = mmu->address_space.WRAM_handler;
  handler->destroy(handler);

  free(mmu);
}

static mem_handler_t *mmu_get_mem_handler(mmu_t *mmu, gb_address_t address) {
  switch (address & 0xE000) {
    case 0x0000:
    case 0x2000:
    case 0x4000:
    case 0x6000:
      return mmu->address_space.ROM_handler;
    case 0x8000:
      return mmu->address_space.VRAM_handler;
    case 0xA000:
      return mmu->address_space.extRAM_handler;
    case 0xC000:
      return mmu->address_space.WRAM_handler;
    default:
      break;
  }

  if (address < 0xFE00)
    /* echo ram */
    return (mem_handler_t *) &mmu->address_space.echo_handler;

  as_handle_t idx = mmu->address_space.high_mem_handles[address - 0xFE00];
  return mmu->address_space.memory_handlers[idx];
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

void mmu_clean(mmu_t *mmu) {
  memset(mmu->internal_ram, 0, sizeof(mmu->internal_ram));
  memset(mmu->high_memory, 0, sizeof(mmu->high_memory));
}

void mmu_dma_transfer(mmu_t *mmu, gb_address_t from, gb_address_t to) {
  for (int i = 160; i--;) {
    mmu_write(mmu, to + i, mmu_read(mmu, from + i));
  }
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
