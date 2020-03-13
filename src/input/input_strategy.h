#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  GAME_BOY_RIGHT  = 1,
  GAME_BOY_LEFT   = 2,
  GAME_BOY_UP     = 4,
  GAME_BOY_DOWN   = 8,
  GAME_BOY_A      = 16,
  GAME_BOY_B      = 32,
  GAME_BOY_SELECT = 64,
  GAME_BOY_START  = 128,
} input_button_t;

typedef struct input_controller input_ctrl_t;
typedef struct input_controller {
  void (*press)(input_ctrl_t *, uint8_t key);
  void (*release)(input_ctrl_t *, uint8_t key);
} input_ctrl_t;

typedef struct input_strategy input_strategy_t;
typedef struct input_strategy {
  bool (*handle_button_press)(input_strategy_t *);
  void (*delete)(input_strategy_t *);

  /* this gets injected automatically and can be used for
   * interface implementation */
  input_ctrl_t *controller;
} input_strategy_t;

