#include "sdl_input.h"
#include <SDL2/SDL.h>
#include <stdio.h>

static uint8_t decode_keycode(SDL_Keycode keycode) {
  uint8_t key = 0;
  switch (keycode) {
    case SDLK_UP:
    case SDLK_k:
      key = GAME_BOY_UP;
      break;
    case SDLK_LEFT:
    case SDLK_h:
      key = GAME_BOY_LEFT;
      break;
    case SDLK_DOWN:
    case SDLK_j:
      key = GAME_BOY_DOWN;
      break;
    case SDLK_RIGHT:
    case SDLK_l:
      key = GAME_BOY_RIGHT;
      break;
    case SDLK_SPACE:
    case SDLK_RETURN:
      key = GAME_BOY_START;
      break;
    case SDLK_x:
      key = GAME_BOY_A;
      break;
    case SDLK_y:
      key = GAME_BOY_B;
      break;
    case SDLK_BACKSPACE:
    case SDLK_c:
      key = GAME_BOY_SELECT;
      break;
    default:
      break;
  }

  return key;
}

bool handle_button_press(input_t *input) {
  SDL_Event event;

  if (!SDL_PollEvent(&event)) return false;

  uint8_t key = decode_keycode(event.key.keysym.sym);

  switch (event.type) {
    case SDL_KEYDOWN:
      input_press(input, key);
      break;

    case SDL_KEYUP: {
      input_unpress(input, key);
      break;
    }

    case SDL_QUIT: {
      fprintf(stderr, "Goodbye.\n");
      return true;
    }

    default:
      break;
  }

  return false;
}

