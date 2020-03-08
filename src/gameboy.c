#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#include <cpu/cpu.h>
#include <memory/mmu.h>
#include <memory/memory_handler.h>
#include <video/ppu.h>
#include <video/display.h>

#include <input/input_strategy.h>

#include "gameboy.h"
#include "cartridge.h"

extern input_ctrl_t *input_ctrl_impl_new(cpu_t *interrupt_line, mmu_t *mmu);

extern void input_ctrl_impl_delete(input_ctrl_t *input);

extern void die(const char *s);

static bool set_up_boot_rom(const char *boot_file);

static void wait_until_next_frame(double time_spent);

typedef struct game_boy_t game_boy_t;

static DEF_MEM_WRITE(default_rom_write) {}

static DEF_MEM_READ(null_read) { return 0; }

static mem_handler_t *null_handler_create(void) {
  return mem_handler_create(null_read, default_rom_write);
}

typedef struct game_boy_t {
  cpu_t cpu;
  input_strategy_t *joy_pad;
  cartridge_t *cartridge;
  display_t *display;

  uint8_t vram[8 * 1024];
} game_boy_t;

/*
 * Creates and returns a new gb_t instance which represents the game boy.
 * The structure should be freed with game_boy_delete().
 * .boot_file       If provided, the boot sequence is run before any cartridge
 *                  runs. The boot file has to contain 256 bytes of code.
 * .surface         The structure the LCD content is drawn on.
 */
gb_t game_boy_new(const char *boot_file, display_t *display,
                  input_strategy_t *input_strategy) {
  game_boy_t *game_boy = calloc(1, sizeof(game_boy_t));
  if (!game_boy) return 0;

  mmu_t *mmu = mmu_new();
  if (!mmu) {
    free(game_boy);
    return 0;
  }

  ppu_t *ppu = ppu_new(mmu, &game_boy->cpu, game_boy->vram, display);
  if (!ppu) {
    free(game_boy);
    free(mmu);
    return 0;
  }

  cpu_init(&game_boy->cpu, mmu, ppu);

  input_strategy->controller = input_ctrl_impl_new(&game_boy->cpu, mmu);

  if (!input_strategy->controller) {
    cpu_delete(&game_boy->cpu);
    return 0;
  }
  game_boy->joy_pad = input_strategy;
  game_boy->display = display;

  if (boot_file && set_up_boot_rom(boot_file))
    enable_boot_rom(mmu);
  else
    game_boy_entry_after_boot(game_boy);

  return game_boy;
}

void game_boy_delete(gb_t gb) {
  cpu_delete(&(gb->cpu));
  input_ctrl_impl_delete(gb->joy_pad->controller);
  gb->joy_pad->delete(gb->joy_pad);
  cartridge_delete(gb->cartridge);
  free(gb);
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
                          const char *save_file) {
  if (gb->cartridge) cartridge_delete(gb->cartridge);
  gb->cartridge = cartridge_new(game_path, save_file);

  mmu_t *mmu = gb->cpu.mmu;

  mem_handler_t *handler = cartridge_get_memory_handler(gb->cartridge);
  mmu_assign_rom_handler(mmu, handler);
  mmu_assign_extram_handler(mmu, handler);

  if (!gb->cartridge) die("Cartridge could not be inserted");
}

/*
 * Start up the game boy and run the game. If no cartridge has been inserted,
 * the game boy will execute only NOPs.
 * .gb          The game boy structure.
 * .data        The structure the LCD draws on.
 * .window      The window that displays the LCD content to the screen.
 */
void game_boy_run(gb_t gb) {
  /* if no cartridge is present, set all the related memory to 0 */
  if (!gb->cartridge) {
    mem_handler_t *handler = null_handler_create();
    if (!handler)
      die("Mem_handler allocation failed");

    mmu_t *mmu = gb->cpu.mmu;
    mmu_assign_rom_handler(mmu, handler);
    mmu_assign_extram_handler(mmu, handler);
    mmu_assign_vram_handler(mmu, handler);
  }

  debugger_t *debugger = 0;

  /* start the main loop */
  clock_t start_t = clock();

#ifdef DEBUG
  clock_t fps_t = start_t;
  uint32_t num_frames = 0;
#endif

  while (true) {
    uint8_t cycles_spent = update_cpu_state(&gb->cpu, debugger);

    if (!ppu_update(gb->cpu.ppu, cycles_spent))
      continue;

    if (gb->joy_pad->handle_button_press(gb->joy_pad)) {
      /* quit game */
      break;
    }

    /* draw to screen*/
    gb->display->show(gb->display);

    wait_until_next_frame((double) (clock() - start_t) / CLOCKS_PER_SEC);

    clock_t now = clock();

#ifdef DEBUG
    ++num_frames;
    double time_passed = (now - fps_t) / CLOCKS_PER_SEC;
    if (time_passed > 0.25 && num_frames > 10) {
      fprintf(stderr, "FPS: %d\n", (int)((double)num_frames / time_passed));
      num_frames = 0;
      fps_t = now;
    }
#endif

    start_t = now;
  }
}

static void wait_until_next_frame(double time_spent) {
  static const unsigned frame_rate = 60;
  time_spent = fmax((1.0 / frame_rate) - time_spent, 0);
  usleep((useconds_t) (time_spent * 1000000));
}
