#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#import "cartridge.h"

void die(const char *s);

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
  const char *save_file_name;
  char save_file_name_buffer[FILENAME_MAX];

  uint8_t *rom_memory;
  uint8_t *ram_memory;

  uint8_t selected_rom_bank;
  uint8_t selected_ram_bank;

  bool ram_enabled;
  enum {ROM_MODE, RAM_MODE} mode;

  cartridge_mem_handler_t internal_mem_handler;
} cartridge_t;

static size_t cartridge_calculate_ram_size(uint8_t ram_size);
#define get_rom_size(cart)  (1 << ((cart)->header->rom_size + 1))
#define upper_bank_no(cart) (uint8_t) ((cart)->selected_rom_bank & 0xE0)
#define lower_bank_no(cart) (uint8_t) ((cart)->selected_rom_bank & 0x1F)

void cartridge_save_game(cartridge_t *cart) {
  FILE * file = fopen(cart->save_file_name, "w");
  if (!file) { die("fopen"); }

  size_t size = cartridge_calculate_ram_size(cart->header->ram_size);
  fwrite(cart->ram_memory, size, 1, file);

  fclose(file);
}

void cartridge_load_game(cartridge_t *cart) {
  FILE * file = fopen(cart->save_file_name, "r");
  if (!file) { perror("Save game could not be loaded"); return; }

  size_t size = cartridge_calculate_ram_size(cart->header->ram_size);
  fread(cart->ram_memory, size, 1, file);

  fclose(file);
}

static uint8_t *cartridge_ram_access(cartridge_t *cart, gb_address_t address) {
  assert(cart->selected_ram_bank < 4);
  uint8_t used_ram_bank = cart->mode ? cart->selected_ram_bank : (uint8_t)0;
  uint8_t *ram_bank = cart->ram_memory + used_ram_bank * 0x2000;
  return ram_bank + (address & ~(0xE000));
}

static DEF_MEM_READ(cartridge_read) {
    cartridge_t *cart = ((cartridge_mem_handler_t *)this)->cart;

    switch (address & 0xE000) {
      case 0x0000: case 0x2000:
        return *(cart->rom_memory + address);

      case 0x4000: case 0x6000:
        return *(cart->rom_memory + (cart->selected_rom_bank - 1) * 0x4000 + address);

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
  static uint8_t max_banks[] = { 0, 0, 0, 3, 15 };
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
    cartridge_t *cart = ((cartridge_mem_handler_t *)this)->cart;

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
        cart->mode = (uint8_t)(value & 0x1);
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
    cartridge_t *cart = ((cartridge_mem_handler_t *)this)->cart;

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
    case 0x1: case 0x2: case 0x3:
      c->internal_mem_handler.base.write = cartridge_mbc1_write;
      break;
    case 0x13:
      c->internal_mem_handler.base.write = cartridge_mbc3_write;
      break;
    default:
      die("Unsupported cartridge type");
  }
}

static void cartridge_print_header(cartridge_header_t *header) {
  fprintf(stderr, "Running: %15s\n", header->game_title);
}

static size_t get_file_size(FILE *file) {
  fseek(file, 0, SEEK_END);
  long size = ftell(file);

  if (size < 0) {
    perror("ftell");
    abort();
  }

  fseek(file, 0, SEEK_SET);
  return (size_t)size;
}

static uint8_t *cartridge_load(const char *filename) {
  FILE *file = fopen(filename, "rb");
  if (!file) {
    perror("fopen");
    return 0;
  }

  size_t rom_size = get_file_size(file);
  uint8_t *memory = malloc(rom_size);

  if (!memory) return 0;

  if (fread(memory, 1, rom_size, file) != rom_size)
    die("Could not read in all bytes from cartridge file.");

  fclose(file);

  return memory;
}

static size_t cartridge_calculate_ram_size(uint8_t ram_size) {
  return ((size_t)1 << (ram_size * 2 - 1)) * 1024;
}

static uint8_t *cartridge_allocate_ram(uint8_t ram_size) {
  return malloc(cartridge_calculate_ram_size(ram_size));
}

static void fill_save_game_name(cartridge_t *cart) {
  size_t len = sizeof(cart->save_file_name_buffer) - 1;
  char *end = stpncpy(cart->save_file_name_buffer, cart->game_file_name, len);
  if (end - cart->save_file_name_buffer + 5 >= FILENAME_MAX)
    die("The save file name is too long. Consider a relative path.");
  strcpy(end, ".save");
}

cartridge_t *cartridge_new(const char *game_path, const char *save_path) {
  cartridge_t *cart = calloc(1, sizeof(cartridge_t));
  if (!cart) return 0;

  uint8_t *memory = cartridge_load(game_path);
  if (!memory) { free(cart); return 0; }

  cart->game_file_name = game_path;
  if (save_path)
    cart->save_file_name = save_path;
  else {
    fill_save_game_name(cart);
    cart->save_file_name = cart->save_file_name_buffer;
  }

  cartridge_header_t *header = (cartridge_header_t *)(memory + 0x100);
  /* TODO: remove this later */
  cartridge_print_header(header);

  cart->rom_memory = memory;
  cart->header = header;
  cart->selected_rom_bank = 1;
  cart->selected_ram_bank = 0;
  cart->ram_enabled = 0;

  if (header->ram_size) {
    cart->ram_memory = cartridge_allocate_ram(header->ram_size);
    if (!cart->ram_memory) { free(memory); free(cart); return 0; }
  }

  cartridge_load_game(cart);

  cartridge_mem_handler_init(cart);

  return cart;
}

mem_handler_t *cartridge_get_memory_handler(cartridge_t *cart) {
  return (mem_handler_t *)&cart->internal_mem_handler;
}

void cartridge_delete(cartridge_t *c) {
  free(c->rom_memory);
  if (c->ram_memory) free(c->ram_memory);
  free(c);
}
