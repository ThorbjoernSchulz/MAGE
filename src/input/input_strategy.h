#pragma once
#include <stdbool.h>
#include <stdint.h>

#define GAME_BOY_START  0x80
#define GAME_BOY_SELECT 0x40
#define GAME_BOY_B      0x20
#define GAME_BOY_A      0x10
#define GAME_BOY_DOWN   0x8
#define GAME_BOY_UP     0x4
#define GAME_BOY_LEFT   0x2
#define GAME_BOY_RIGHT  0x1

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

