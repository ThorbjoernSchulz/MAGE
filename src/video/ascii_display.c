#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ascii_display.h"

#include <logging.h>

static const int scale = 2;
static uint8_t buffer[144 / scale + 1][160 / scale + 1];

static const char *map = "@MkmCcj(]<;. ";

#define round(x) (int)((x) + 0.5f)

typedef struct ascii_display {
  display_t base;
  int line_counter;
} ascii_display_t;

void ascii_display_show(display_t *this) {
  printf("\e[1;1H\e[2J%s", (char*) buffer);
  memset(buffer, 0, sizeof(buffer));
}

void ascii_display_draw_line(display_t *this, uint8_t *line) {
  ascii_display_t *display = (ascii_display_t *)this;

  if (display->line_counter == 144)
    display->line_counter = 0;

  int y = display->line_counter / scale;

  for (int i = 0; i < 160; ++i)
    buffer[y][i / scale] += line[i];

  if (display->line_counter % scale == scale - 1) {
    for (int i = 0; i < 160 / scale; ++i) {
      int average = round(buffer[y][i]);
      buffer[y][i] = map[average];
    }

    buffer[y][160 / scale] = '\n';
  }

  ++display->line_counter;
}

void ascii_display_delete(display_t *display) {
  free(display);
}

display_t *ascii_display_new(void) {
  ascii_display_t *display = calloc(1, sizeof(ascii_display_t));
  if (!display) {
    logging_std_error();
    return 0;
  }

  display->base.show = ascii_display_show;
  display->base.draw_line = ascii_display_draw_line;
  display->base.delete = ascii_display_delete;

  return (display_t *)display;
}

