#include "sdl_display.h"

#include <SDL2/SDL.h>

#include <logging.h>

typedef struct sdl_display {
  display_t base;
  int line_counter;
  SDL_Window *window;
  SDL_Surface *surface;
} sdl_display_t;

void sdl_display_draw_line(display_t *this, uint8_t *line) {
  sdl_display_t *display = (sdl_display_t *) this;

  static const uint32_t to_sdl_color[] = {
      0x00FFFFFF, 0x00AAAAAA, 0x00555555, 0x00000000
  };
  uint32_t *pixel_buffer = (uint32_t *) display->surface->pixels;
  pixel_buffer += display->line_counter++ * 160;

  if (display->line_counter == 144)
    display->line_counter = 0;

  for (int i = 0; i < 160; ++i)
    pixel_buffer[i] = to_sdl_color[line[i]];
}

void sdl_display_show(display_t *this) {
  sdl_display_t *display = (sdl_display_t *) this;
  if (SDL_BlitScaled(display->surface, NULL,
                     SDL_GetWindowSurface(display->window), NULL)) {
    logging_error(SDL_GetError());
    abort();
  }
  SDL_UpdateWindowSurface(display->window);
}

void sdl_display_delete(display_t *this) {
  sdl_display_t *display = (sdl_display_t *) this;
  SDL_FreeSurface(display->surface);
  SDL_DestroyWindow(display->window);
  free(this);
}

display_t *sdl_display_new(void) {
  sdl_display_t *display = calloc(1, sizeof(sdl_display_t));
  if (!display) {
    logging_std_error();
    return 0;
  }

  SDL_Window *window = SDL_CreateWindow("GameBoy",
                                        SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED, 640, 576, 0);
  atexit(SDL_Quit);

  if (!window) goto sdl_fail;

  SDL_Surface *window_surface = SDL_GetWindowSurface(window);
  if (!window_surface) goto sdl_fail;

  SDL_Surface *surface = SDL_CreateRGBSurface(
      0, 160, 144,
      window_surface->format->BitsPerPixel,
      window_surface->format->Rmask,
      window_surface->format->Gmask,
      window_surface->format->Bmask,
      window_surface->format->Amask);

  if (!surface) goto sdl_fail;

  display->base.draw_line = sdl_display_draw_line;
  display->base.show = sdl_display_show;
  display->base.delete = sdl_display_delete;

  display->window = window;
  display->surface = surface;

  display->line_counter = 0;

  return (display_t *) display;

  sdl_fail:
  free(display);
  logging_error(SDL_GetError());
  return 0;
}

