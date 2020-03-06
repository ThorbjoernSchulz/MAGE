#pragma once
#include <stdbool.h>

typedef struct input_device input_device_t;
typedef struct input_controller input_ctrl_t;

typedef struct input_controller {
  bool (*handle_button_press)(input_ctrl_t *);
  void (*delete)(input_ctrl_t *);
  input_device_t *device;
} input_ctrl_t;
