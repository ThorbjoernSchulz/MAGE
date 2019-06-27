#include <stdlib.h>
#include <stdio.h>

#include "gameboy.h"
#include "cpu.h"
#include "mmu.h"

#include "memory_handler.h"
#include "interrupts.h"
#include "cartridge.h"

#include <SDL2/SDL.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

void die(const char *s);

typedef struct game_boy_t game_boy_t;

static DEF_MEM_WRITE(default_rom_write) {}
static DEF_MEM_READ(null_read) { return 0; }

static mem_handler_t *null_handler_create(void) {
  return mem_handler_create(null_read, default_rom_write);
}

typedef struct vram_handler {
  mem_handler_t base;
  uint8_t *video_ram;
} vram_handler_t;

static DEF_MEM_WRITE(vram_write) {
  vram_handler_t *handler = (vram_handler_t *) this;
  handler->video_ram[address & ~(0xE000)] = value;
}

static DEF_MEM_READ(vram_read) {
  vram_handler_t *handler = (vram_handler_t *) this;
  return handler->video_ram[address & ~(0xE000)];
}

mem_handler_t *vram_handler_create(uint8_t *vram) {
  vram_handler_t *handler = calloc(1, (sizeof(vram_handler_t)));
  if (!handler) return 0;

  handler->base.write = vram_write;
  handler->base.read = vram_read;
  handler->base.destroy = mem_handler_default_destroy;
  handler->video_ram = vram;
  return (mem_handler_t *)handler;
}

/* # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
 *     GAME BOY
 * # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # */

typedef struct game_boy_t {
  cpu_t cpu;
  cartridge_t *cartridge;

  uint8_t vram[8 * 1024];
} game_boy_t;

void game_boy_insert_game(gb_t gb, const char *game_path,
                                   const char *save_path) {
  if (gb->cartridge) cartridge_delete(gb->cartridge);
  gb->cartridge = cartridge_new(game_path, save_path);

  mmu_t *mmu = gb->cpu.mmu;
  as_handle_t rom = mmu_map_memory(mmu, 0x0000, 0x8000);
  as_handle_t ram = mmu_map_memory(mmu, 0xA000, 0xC000);

  mem_handler_t *handler = cartridge_get_memory_handler(gb->cartridge);
  mmu_register_mem_handler(mmu, handler, rom);
  mmu_register_mem_handler(mmu, handler, ram);

  if (!gb->cartridge) die("Cartridge could not be inserted");
}

void game_boy_delete(gb_t gb) {
  cpu_delete(&(gb->cpu));
  cartridge_delete(gb->cartridge);
  free(gb);
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

gb_t game_boy_new(const char *boot_file, SDL_Surface *surface) {
  game_boy_t *game_boy = calloc(1, sizeof(game_boy_t));
  if (!game_boy) return 0;

  mmu_t *mmu = mmu_new();
  if (!mmu) {
    free(game_boy);
    return 0;
  }

  lcd_t *lcd = lcd_new(mmu, game_boy->vram, surface);
  if (!lcd) {
    free(game_boy);
    free(mmu);
    return 0;
  }

  cpu_init(&game_boy->cpu, mmu, lcd);

  mem_handler_t *handler = vram_handler_create(game_boy->vram);
  if (!handler) die("Could not allocate video_ram memory");
  mmu_register_vram(mmu, handler);

  if (boot_file) {
    FILE *stream = fopen(boot_file, "r");
    if (stream) {
      size_t size = get_file_size(stream);
      if (size == 256) {
        load_boot_rom(stream);
        fclose(stream);
        enable_boot_rom(mmu);
        return game_boy;
      }

      fprintf(stderr, "Boot-ROM needs to contain 256 bytes.\n");
    }
    else
      perror(boot_file);
  }

  game_boy_entry_after_boot(game_boy);
  return game_boy;
}

extern void mmu_init_after_boot(mmu_t *mmu);
void game_boy_entry_after_boot(gb_t gb) {
  cpu_t *cpu = &(gb->cpu);

  cpu->pc = 0x100;
  cpu->S = 0xFF;
  cpu->P = 0xFE;

  cpu->A = 0x01;
  cpu->F = 0xB0;
  cpu->B = 0x00;
  cpu->C = 0x13;
  cpu->D = 0x00;
  cpu->E = 0xD8;
  cpu->H = 0x01;
  cpu->L = 0x4D;

  mmu_init_after_boot(cpu->mmu);
}

bool handle_button_press(cpu_t *cpu);

void game_boy_run(gb_t gb, SDL_Surface *data, SDL_Window *window) {
  if (!gb->cartridge) {
    mem_handler_t *handler = null_handler_create();
    if (!handler)
      die("Mem_handler allocation failed");

    as_handle_t idx = mmu_map_memory(gb->cpu.mmu, 0x0, 0xC000);
    mmu_register_mem_handler(gb->cpu.mmu, handler, idx);
  }

  debugger_t *debugger = 0;

  clock_t start_t, end_t;
  start_t = clock();

  while (true) {
    uint8_t cycles_spent = cpu_update_state(&gb->cpu, debugger);

    if (!run_lcd(&gb->cpu, cycles_spent))
      continue;

    if (handle_button_press(&gb->cpu)) {
      /* quit game */
      break;
    }

    if (SDL_BlitScaled(data, NULL, SDL_GetWindowSurface(window), NULL)) {
      die(SDL_GetError());
    }

    SDL_UpdateWindowSurface(window);
    end_t = clock();
    double time_spent = (double) (end_t - start_t) / CLOCKS_PER_SEC;
    time_spent = (1.0 / 60) - time_spent;
    if (time_spent < 0) time_spent = 1;
    usleep((useconds_t) (time_spent * 1000000));
    start_t = clock();
  }

  cartridge_save_game(gb->cartridge);
}

