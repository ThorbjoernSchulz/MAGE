#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_timer.h>

void die(const char *s) {
  fputs(s, stderr);
  abort();
}

#include "gameboy.h"
#include <getopt.h>

static struct {
  char *file_name;
  char *boot_rom;
  char *save_file;
} set_options;

static struct option options[] = {
    {"help", no_argument, 0, 'h'},
    {"file", required_argument, 0, 'f'},
    {"boot_rom", required_argument, 0, 'b'},
    {"save", required_argument, 0, 's'},
    {NULL, 0, NULL, 0},
};

static const char *option_string = "h:f:b:s:";

static void usage(const char *program_name) {
  fprintf(stderr, "Usage: %s --file FILE [OPTIONS]\n", program_name);
  fprintf(stderr, "Available Options:\n");
  fprintf(stderr, "\t-h,--help             Display this message.\n");
  fprintf(stderr, "\t-f,--file FILE        Specify game file.\n");
  fprintf(stderr, "\t-b,--boot_rom FILE    Enable boot screen.\n");
  fprintf(stderr, "\t-s,--save FILE        Specify save game file.\n");
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
  free(set_options.file_name);
  free(set_options.boot_rom);
  free(set_options.save_file);
}

int main(int argc, char *argv[]) {
  if (setup_options(argc, argv)) { usage(argv[0]); return 1; }

  /* Let's start up the visual interface */
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    die(SDL_GetError());
  }

  SDL_Window *window = SDL_CreateWindow("GameBoy",
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 576, 0);
  atexit(SDL_Quit);

  if(!window) {
    die(SDL_GetError());
    return 1;
  }

  SDL_Surface *window_surface = SDL_GetWindowSurface(window);
  if (!window_surface) {
    die(SDL_GetError());
    return 1;
  }

  SDL_Surface *surface = SDL_CreateRGBSurface(
      0, 160, 144,
      window_surface->format->BitsPerPixel,
      window_surface->format->Rmask,
      window_surface->format->Gmask,
      window_surface->format->Bmask,
      window_surface->format->Amask);

  if (!surface) {
    die(SDL_GetError());
  }

  /* Ok, now that we have something to draw on, let us start the emulator */
  gb_t gb = game_boy_new(set_options.boot_rom, surface);
  game_boy_insert_game(gb, set_options.file_name, set_options.save_file);
  game_boy_run(gb, surface, window);

  /* Clean everything up */
  SDL_DestroyWindow(window);
  SDL_FreeSurface(surface);
  game_boy_delete(gb);
  options_delete();
  return 0;
}