#pragma once

#include <stdint.h>

typedef struct display display_t;
typedef struct display {
  void (*draw_line)(display_t *this, uint8_t *line);
  void (*show)(display_t *this);
  void (*delete)(display_t *this);
} display_t ;
