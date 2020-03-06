#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <video/sdl_display.h>
#include <SDL2/SDL.h>

#include "gameboy.h"
#include "logging.h"

/* Merely Another Game Boy Emulator, created by Thorbjörn Schulz */

void die(const char *s) {
  fputs(s, stderr);
  abort();
}

static struct {
  const char *file_name;
  const char *boot_rom;
  const char *save_file;
  bool no_save;
} set_options;

static struct option options[] = {
    {"help",     no_argument,       0, 'h'},
    {"file",     required_argument, 0, 'f'},
    {"boot_rom", required_argument, 0, 'b'},
    {"save",     required_argument, 0, 's'},
    {"no-save",  no_argument,       0, 'n'},
    {NULL, 0, NULL,                    0},
};

static const char *option_string = "h:f:b:s:n";

static void usage(const char *program_name) {
  fprintf(stderr, "Usage: %s --file FILE [OPTIONS]\n", program_name);
  fprintf(stderr, "Available Options:\n");
  fprintf(stderr, "\t-h,--help             Display this message.\n");
  fprintf(stderr, "\t-f,--file FILE        Specify game file.\n");
  fprintf(stderr, "\t-b,--boot_rom FILE    Enable boot screen.\n");
  fprintf(stderr, "\t-s,--save FILE        Specify save game file.\n");
  fprintf(stderr, "\t-n,--no-save          No save game generation.\n");
}

static int setup_options(int argc, char *argv[]) {
  /* parse command line options */
  char flg;
  while ((flg = getopt_long(argc, argv, option_string, options, 0)) != -1) {
    switch (flg) {
      case 'h':
        usage(argv[0]);
        exit(0);
      case 'f':
        set_options.file_name = strdup(optarg);
        break;
      case 'b':
        set_options.boot_rom = strdup(optarg);
        break;
      case 's':
        set_options.save_file = strdup(optarg);
        break;
      case 'n':
        set_options.no_save = true;
        break;
      case '?':
        return 1;
      default:
        break;
    }
  }

  if (!set_options.file_name) return 1;

  return 0;
}

static void options_delete(void) {
  free((void *) set_options.file_name);
  free((void *) set_options.boot_rom);
  free((void *) set_options.save_file);
}

int main(int argc, char *argv[]) {
  logging_initialize();

  if (setup_options(argc, argv)) {
    usage(argv[0]);
    return 1;
  }

  /* Let's start up the visual interface */
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    logging_error(SDL_GetError());
    return 1;
  }

  display_t *display = sdl_display_new();

  /* Ok, now that we have something to draw on, let us start the emulator */
  gb_t gb = game_boy_new(set_options.boot_rom, display);

  if (!set_options.no_save && !set_options.save_file) {
    set_options.save_file = "default.save";
    logging_warning(
        "No save file specified, using 'default.save' as a fallback.");
  }

  game_boy_insert_game(gb, set_options.file_name, set_options.save_file);
  game_boy_run(gb);

  /* Clean everything up */
  game_boy_delete(gb);
  display->delete(display);
  options_delete();
  return 0;
}
