#include <SDL2/SDL.h>
#include <logging.h>
#include <assert.h>
#include "input_strategy.h"

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

static bool handle_button_press(input_strategy_t *this) {
  assert(this->controller);
  SDL_Event event;

  if (!SDL_PollEvent(&event)) return false;

  uint8_t key = decode_keycode(event.key.keysym.sym);

  switch (event.type) {
    case SDL_KEYDOWN:
      this->controller->press(this->controller, key);
      break;

    case SDL_KEYUP: {
      this->controller->release(this->controller, key);
      break;
    }

    case SDL_QUIT: {
      logging_message("Goodbye.");
      return true;
    }

    default:
      break;
  }

  return false;
}

void sdl_joy_pad_delete(input_strategy_t *this) {
  /* TODO came back to this */
  free(this);
}

input_strategy_t *sdl_joy_pad_new(void) {
  input_strategy_t *strategy = calloc(1, sizeof(input_strategy_t));
  if (!strategy) {
    logging_std_error();
    return 0;
  }

  strategy->handle_button_press = handle_button_press;
  strategy->delete = sdl_joy_pad_delete;

  return strategy;
}
