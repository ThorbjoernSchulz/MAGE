#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#import "cartridge.h"
#include "logging.h"

void die(const char *s);

typedef struct save_game save_game_t;

typedef struct save_game {
  const char *file_name;
  void (*save)(save_game_t *);
  void (*load)(save_game_t *);

  cartridge_t *cart;
} save_game_t;

static void save_game_load(save_game_t *this);

static void save_game_save(save_game_t *this);

static void save_game_nop(save_game_t *this) {}

save_game_t *save_game_new(const char *file_name, cartridge_t *cart) {
  save_game_t *sg = calloc(1, sizeof(save_game_t));
  if (!sg) return 0;

  sg->cart = cart;

  sg->file_name = file_name;
  if (file_name) {
    sg->save = save_game_save;
    sg->load = save_game_load;
  }
  else {
    sg->save = save_game_nop;
    sg->load = save_game_nop;
  }

  return sg;
}

void save_game_delete(save_game_t *this) {
  /* save upon deletion */
  this->save(this);
  free(this);
}

typedef struct __attribute__((packed)) cartridge_header_t {
  uint8_t __[52];
  uint8_t game_title[15];
  uint8_t color_gb;
  uint8_t license[2];
  uint8_t gb_or_sgb;
  uint8_t cartridge_type;
  uint8_t rom_size;
  uint8_t ram_size;
  uint8_t ___[6];
} cartridge_header_t;

typedef struct cartridge_mem_handler_t {
  mem_handler_t base;
  cartridge_t *cart;
} cartridge_mem_handler_t;

typedef struct cartridge_t {
  cartridge_header_t *header;
  const char *game_file_name;
  save_game_t *save_game;

  uint8_t *rom_memory;
  uint8_t *ram_memory;

  uint8_t selected_rom_bank;
  uint8_t selected_ram_bank;

  bool ram_enabled;
  enum {
    ROM_MODE, RAM_MODE
  } mode;

  cartridge_mem_handler_t internal_mem_handler;
} cartridge_t;

static size_t cartridge_calculate_ram_size(uint8_t ram_size);

#define get_rom_size(cart)  (1 << ((cart)->header->rom_size + 1))
#define upper_bank_no(cart) (uint8_t) ((cart)->selected_rom_bank & 0xE0)
#define lower_bank_no(cart) (uint8_t) ((cart)->selected_rom_bank & 0x1F)

static void save_game_load(save_game_t *this) {
  FILE *file = fopen(this->file_name, "r");
  if (!file) {
    logging_warning("Save game could not be loaded");
    return;
  }

  cartridge_t *cart = this->cart;
  size_t size = cartridge_calculate_ram_size(cart->header->ram_size);
  fread(cart->ram_memory, size, 1, file);

  fclose(file);
}

static void save_game_save(save_game_t *this) {
  FILE *file = fopen(this->file_name, "w");
  if (!file) {
    logging_warning("Save game could not be loaded");
    return;
  }

  logging_message("Saving game.");

  cartridge_t *cart = this->cart;
  size_t size = cartridge_calculate_ram_size(cart->header->ram_size);
  fwrite(cart->ram_memory, size, 1, file);

  fclose(file);
}

static uint8_t *cartridge_ram_access(cartridge_t *cart, gb_address_t address) {
  assert(cart->selected_ram_bank < 4);
  uint8_t used_ram_bank = cart->mode ? cart->selected_ram_bank : (uint8_t) 0;
  uint8_t *ram_bank = cart->ram_memory + used_ram_bank * 0x2000;
  return ram_bank + (address & ~(0xE000));
}

static DEF_MEM_READ(cartridge_read) {
  cartridge_t *cart = ((cartridge_mem_handler_t *) this)->cart;

  switch (address & 0xE000) {
    case 0x0000:
    case 0x2000:
      return *(cart->rom_memory + address);

    case 0x4000:
    case 0x6000:
      return *(cart->rom_memory + (cart->selected_rom_bank - 1) * 0x4000 +
               address);

    case 0xA000: {
      uint8_t value = 0xFF;
      if (cart->ram_enabled)
        value = (*cartridge_ram_access(cart, address));
      return value;
    }

    default:
      die("Illegal address");
  }

  return 0;
}

/* Memory Bank Controller 1 */

static void select_ram_bank(cartridge_t *cart, uint8_t bank_no) {
  static uint8_t max_banks[] = {0, 0, 0, 3, 15};
  uint8_t max_bank_no = max_banks[cart->header->ram_size];
  cart->selected_ram_bank = bank_no > max_bank_no ? max_bank_no : bank_no;
}

static void select_rom_bank(cartridge_t *cart, uint8_t bank_no) {
  uint8_t mask = (uint8_t) (get_rom_size(cart) - 1);
  cart->selected_rom_bank = bank_no & mask;
}

static void mbc1_rom0_write(cartridge_t *cart, uint8_t value) {
  uint8_t bank_no = (uint8_t) (value & 0x1F);
  if (bank_no == 0x0) ++bank_no;

  if (cart->mode == ROM_MODE)
    bank_no = (upper_bank_no(cart) | bank_no);

  select_rom_bank(cart, bank_no);
}

static void mbc1_rom_switch_write(cartridge_t *cart, uint8_t value) {
  value &= 3;
  if (cart->mode == ROM_MODE)
    select_rom_bank(cart, lower_bank_no(cart) | (value << 5));
  else
    select_ram_bank(cart, value);
}

static DEF_MEM_WRITE(cartridge_mbc1_write) {
  cartridge_t *cart = ((cartridge_mem_handler_t *) this)->cart;

  switch (address & 0xE000) {
    case 0x0000:
      cart->ram_enabled = (value & 0xF) == 0xA;
      break;

    case 0x2000:
      mbc1_rom0_write(cart, value);
      break;

    case 0x4000:
      mbc1_rom_switch_write(cart, value);
      break;

    case 0x6000:
      cart->mode = (uint8_t) (value & 0x1);
      if (cart->mode == ROM_MODE)
        cart->selected_ram_bank = 0;
      else
        cart->selected_rom_bank = lower_bank_no(cart);
      break;

    case 0xA000: {
      if (cart->ram_enabled)
        *cartridge_ram_access(cart, address) = value;
      break;
    }

    default:
      die("Illegal address");
  }
}

/* Memory Bank Controller 3 */

static DEF_MEM_WRITE(cartridge_mbc3_write) {
  cartridge_t *cart = ((cartridge_mem_handler_t *) this)->cart;

  switch (address & 0xE000) {
    case 0x0000:
      cart->ram_enabled = (value & 0xF) == 0xA;
      break;

    case 0x2000:
      value &= 0x7F;
      if (!value) ++value;
      select_rom_bank(cart, value);
      break;

    case 0x4000:
      select_ram_bank(cart, value);
      break;

    case 0x6000:
      break;

    case 0xA000: {
      if (cart->ram_enabled)
        *cartridge_ram_access(cart, address) = value;
      break;
    }

    default:
      die("Illegal address");
  }
}

static DEF_MEM_WRITE(default_rom_write) {}

static void cartridge_mem_handler_init(cartridge_t *c) {
  c->internal_mem_handler.base.read = cartridge_read;
  c->internal_mem_handler.base.destroy = mem_handler_stack_destroy;
  c->internal_mem_handler.cart = c;

  switch (c->header->cartridge_type) {
    case 0x0:
      c->internal_mem_handler.base.write = default_rom_write;
      break;
    case 0x1:
    case 0x2:
    case 0x3:
      c->internal_mem_handler.base.write = cartridge_mbc1_write;
      break;
    case 0x13:
      c->internal_mem_handler.base.write = cartridge_mbc3_write;
      break;
    default:
      die("Unsupported cartridge type");
  }
}

static void print_running_message(cartridge_header_t *header) {
  fprintf(stderr, "Log: Running: %15s\n", header->game_title);
}

static ssize_t get_file_size(FILE *file) {
  fseek(file, 0, SEEK_END);
  long size = ftell(file);

  if (size < 0) {
    return size;
  }

  fseek(file, 0, SEEK_SET);
  return (size_t) size;
}

static uint8_t *cartridge_load_rom(const char *filename) {
  FILE *file = 0;
  uint8_t *memory = 0;

  file = fopen(filename, "rb");
  if (!file) goto fail;

  ssize_t rom_size = get_file_size(file);
  if (rom_size < 0) goto fail;

  memory = malloc(rom_size);
  if (!memory) goto fail;

  if (fread(memory, 1, rom_size, file) != rom_size) {
    goto fail;
  }

  fclose(file);

  return memory;

  fail:
  logging_std_error();
  free(memory);
  if (file) fclose(file);
  return 0;
}

static size_t cartridge_calculate_ram_size(uint8_t ram_size) {
  return ((size_t) 1 << (ram_size * 2 - 1)) * 1024;
}

void cartridge_delete(cartridge_t *c) {
  /* deleting the save game implicitly saves the game,
   * so this needs to be deleted first */
  save_game_delete(c->save_game);

  free(c->rom_memory);
  if (c->ram_memory) free(c->ram_memory);
  free(c);
}

static uint8_t *cartridge_allocate_ram(uint8_t ram_size) {
  return malloc(cartridge_calculate_ram_size(ram_size));
}

cartridge_t *cartridge_new(const char *game_path, const char *save_file) {
  cartridge_t *cart = 0;
  uint8_t *memory = 0;

  cart = calloc(1, sizeof(cartridge_t));
  if (!cart) goto fail;

  memory = cartridge_load_rom(game_path);
  if (!memory) goto fail;

  cartridge_header_t *header = (cartridge_header_t *) (memory + 0x100);

  cart->rom_memory = memory;
  cart->header = header;
  cart->selected_rom_bank = 1;
  cart->selected_ram_bank = 0;
  cart->ram_enabled = 0;

  if (header->ram_size) {
    cart->ram_memory = cartridge_allocate_ram(header->ram_size);
    if (!cart->ram_memory) goto fail;
  }

  cart->game_file_name = game_path;

  cart->save_game = save_game_new(save_file, cart);
  if (!cart->save_game) goto fail;

  /* load save game into ram */
  cart->save_game->load(cart->save_game);

  cartridge_mem_handler_init(cart);

  print_running_message(header);

  return cart;

fail:
  free(memory);
  if (cart)
    free(cart->save_game);
  free(cart);
  return 0;
}

mem_handler_t *cartridge_get_memory_handler(cartridge_t *cart) {
  return (mem_handler_t *) &cart->internal_mem_handler;
}

void cartridge_delete(cartridge_t *c);
