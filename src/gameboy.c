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
#include <math.h>

void die(const char *s);

bool handle_button_press(cpu_t *cpu);

void set_up_vram(cpu_t *cpu, uint8_t *vram);

static bool set_up_boot_rom(const char *boot_file);

static void wait_until_next_frame(double time_spent);

typedef struct game_boy_t game_boy_t;

static DEF_MEM_WRITE(default_rom_write) {}

static DEF_MEM_READ(null_read) { return 0; }

static mem_handler_t *null_handler_create(void) {
  return mem_handler_create(null_read, default_rom_write);
}

/* # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
 *     GAME BOY
 * # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # */

typedef struct game_boy_t {
  cpu_t cpu;
  cartridge_t *cartridge;

  uint8_t vram[8 * 1024];
} game_boy_t;

/*
 * Creates and returns a new gb_t instance which represents the game boy.
 * The structure should be freed with game_boy_delete().
 * .boot_file       If provided, the boot sequence is run before any cartridge
 *                  runs. The boot file has to contain 256 bytes of code.
 * .surface         The structure the LCD content is drawn on.
 */
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

  set_up_vram(&game_boy->cpu, game_boy->vram);

  if (boot_file && set_up_boot_rom(boot_file))
    enable_boot_rom(mmu);
  else
    game_boy_entry_after_boot(game_boy);

  return game_boy;
}

/*
 * Set up the boot rom. Returns false if an error occured.
 * .boot_file       The file containing the boot code
 */
static bool set_up_boot_rom(const char *boot_file) {
  FILE *stream = fopen(boot_file, "r");
  if (!stream) {
    perror(boot_file);
    return false;
  }

  /* get file size */
  fseek(stream, 0, SEEK_END);
  long size = ftell(stream);

  if (size < 0) {
    perror("ftell");
    fclose(stream);
    return false;
  }

  fseek(stream, 0, SEEK_SET);

  if (size != 256) {
    fprintf(stderr, "Boot-ROM needs to contain 256 bytes.\n");
    return false;
  }

  load_boot_rom(stream);
  fclose(stream);
  return true;
}

extern void mmu_init_after_boot(mmu_t *mmu);

/*
 * Set the internal state of the game boy as it is expected after boot.
 * .gb      The game boy structure.
 */
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

/*
 * Start up the game boy and run the game. If no cartridge has been inserted,
 * the game boy will execute only NOPs.
 * .gb          The game boy structure.
 * .data        The structure the LCD draws on.
 * .window      The window that displays the LCD content to the screen.
 */
void game_boy_run(gb_t gb, SDL_Surface *data, SDL_Window *window) {
  /* if no cartridge is present, set all the related memory to 0 */
  if (!gb->cartridge) {
    mem_handler_t *handler = null_handler_create();
    if (!handler)
      die("Mem_handler allocation failed");

    as_handle_t idx = mmu_map_memory(gb->cpu.mmu, 0x0, 0xC000);
    mmu_register_mem_handler(gb->cpu.mmu, handler, idx);
  }

  debugger_t *debugger = 0;

  /* start the main loop */
  clock_t start_t = clock();

  while (true) {
    uint8_t cycles_spent = update_cpu_state(&gb->cpu, debugger);

    if (!run_lcd(&gb->cpu, cycles_spent))
      continue;

    if (handle_button_press(&gb->cpu)) {
      /* quit game */
      break;
    }

    /* draw to screen, scaled up */
    if (SDL_BlitScaled(data, NULL, SDL_GetWindowSurface(window), NULL)) {
      die(SDL_GetError());
    }
    SDL_UpdateWindowSurface(window);

    wait_until_next_frame((double) (clock() - start_t) / CLOCKS_PER_SEC);
    start_t = clock();
  }

  /* save the game before exit */
  cartridge_save_game(gb->cartridge);
}

static void wait_until_next_frame(double time_spent) {
  static const unsigned frame_rate = 60;
  time_spent = fmax((1.0 / frame_rate) - time_spent, 0);
  usleep((useconds_t) (time_spent * 1000000));
}

void game_boy_delete(gb_t gb) {
  cpu_delete(&(gb->cpu));
  cartridge_delete(gb->cartridge);
  free(gb);
}

/* # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
 *     VRAM
 * # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # */

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
  return (mem_handler_t *) handler;
}

void set_up_vram(cpu_t *cpu, uint8_t *vram) {
  mem_handler_t *handler = vram_handler_create(vram);
  if (!handler) die("Could not allocate video_ram memory");
  mmu_register_vram(cpu->mmu, handler);
}

