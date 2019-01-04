#include <SDL2/SDL.h>
#include <assert.h>
#include "cpu.h"
#include "mmu.h"
#include "interrupts.h"

#define GAME_BOY_START  0x80
#define GAME_BOY_SELECT 0x40
#define GAME_BOY_B      0x20
#define GAME_BOY_A      0x10
#define GAME_BOY_DOWN   0x8
#define GAME_BOY_UP     0x4
#define GAME_BOY_LEFT   0x2
#define GAME_BOY_RIGHT  0x1

static uint8_t joypad_state = 0;

/* Select the relevant bits. Does the game query direction keys or buttons? */
uint8_t get_joypad_state(bool direction) {
  int relevant = (direction ? (joypad_state & 0x0F) : (joypad_state >> 4));
  return (uint8_t)(~relevant);
}

uint8_t decode_keycode(SDL_Keycode keycode) {
  uint8_t key = 0;
  switch (keycode) {
    case SDLK_UP:
      key = GAME_BOY_UP;
      break;
    case SDLK_LEFT:
      key = GAME_BOY_LEFT;
      break;
    case SDLK_DOWN:
      key = GAME_BOY_DOWN;
      break;
    case SDLK_RIGHT:
      key = GAME_BOY_RIGHT;
      break;
    case SDLK_SPACE:
      key = GAME_BOY_START;
      break;
    case SDLK_y:
      key = GAME_BOY_A;
      break;
    case SDLK_x:
      key = GAME_BOY_B;
      break;
    case SDLK_BACKSPACE:
      key = GAME_BOY_SELECT;
      break;
    default:
      break;
  }

  return key;
}

bool handle_button_press(cpu_t *cpu) {
  SDL_Event event;

  if (!SDL_PollEvent(&event)) return false;

  uint8_t key = 0;

  switch (event.type) {
    case SDL_KEYDOWN:
      key = decode_keycode(event.key.keysym.sym);
      joypad_state |= key;
      raise_interrupt(cpu, INT_JOYPAD);
      break;

    case SDL_KEYUP: {
      mmu_set_joypad_input(cpu->mmu, 0xFF);
      key = decode_keycode(event.key.keysym.sym);
      joypad_state &= ~key;
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
