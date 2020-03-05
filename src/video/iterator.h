#pragma once

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

typedef struct sprite_pixel_iterator {
  int x, y;
  px_data_t *tile;
} sprite_px_it_t;

typedef px_data_t *(*find_tile_t)(uint8_t, px_data_t *);

px_data_t *find_tile_unsigned(uint8_t tile_id, px_data_t *tiles);

px_data_t *find_tile_signed(uint8_t tile_id, px_data_t *tiles);

uint8_t next_pixel(camera_iterator_t *it, find_tile_t find_tile,
                   uint8_t *palette);

camera_iterator_t reset_iterator(ppu_regs_t *regs, uint8_t *vram);

void update_scrolling(camera_iterator_t *it, ppu_regs_t *regs);

int next_sprite_pixel(sprite_px_it_t *it, bool x_flip, bool y_flip);

uint16_t decode_tile_line(px_data_t *tile, int line);
uint16_t flip_line(uint16_t line);
