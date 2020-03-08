#pragma once

#define get_pixel(tile_line, n) (((tile_line) >> (14 - n * 2)) & 3)
#define flip_line_8bit(line) (((line) & 3) << 6) | (((line) &  12) << 2)\
                                                 | (((line) &  48) >> 2)\
                                                 | (((line) & 192) >> 6)

typedef struct pixel_data {
  uint8_t data[16];
} px_data_t;

typedef struct camera_iterator {
  /* pixel position on screen */
  int screen_pixel_x;
  int screen_pixel_y;
  /* scroll position in pixel */
  int scroll_x;
  int scroll_y;

  int window_x;
  int window_y;

  px_data_t *tiles;
  uint8_t *display;
  uint8_t *window;
} camera_iterator_t;

typedef px_data_t *(*find_tile_t)(uint8_t, px_data_t *);

px_data_t *find_tile_unsigned(uint8_t tile_id, px_data_t *const tiles);

px_data_t *find_tile_signed(uint8_t tile_id, px_data_t *const tiles);

uint8_t next_pixel(camera_iterator_t *const it, find_tile_t find_tile,
                   uint8_t *const palette);

camera_iterator_t reset_iterator(ppu_regs_t *const regs, uint8_t *const vram);

void update_scrolling(camera_iterator_t *const it, ppu_regs_t *regs);

uint16_t decode_tile_line(px_data_t *const tile, int line);
uint16_t flip_line(uint16_t line);
