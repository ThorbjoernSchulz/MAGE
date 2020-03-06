#include <SDL2/SDL.h>
#include <logging.h>
#include "sdl_input.h"
#include "input_device.h"

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

static bool handle_button_press(input_ctrl_t *input) {
  SDL_Event event;

  if (!SDL_PollEvent(&event)) return false;

  uint8_t key = decode_keycode(event.key.keysym.sym);

  switch (event.type) {
    case SDL_KEYDOWN:
      input_press(input->device, key);
      break;

    case SDL_KEYUP: {
      input_release(input->device, key);
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

void sdl_joy_pad_delete(input_ctrl_t *this) {
  input_delete(this->device);
  free(this);
}

input_ctrl_t *sdl_joy_pad_new(mmu_t *mmu, cpu_t *interrupt_line) {
  input_ctrl_t *controller = calloc(1, sizeof(input_ctrl_t));
  if (!controller) {
    logging_std_error();
    return 0;
  }

  controller->handle_button_press = handle_button_press;
  controller->delete = sdl_joy_pad_delete;
  controller->device = input_new(interrupt_line, mmu);
  if (!controller->device) {
    free(controller);
    return 0;
  }

  return controller;
}
