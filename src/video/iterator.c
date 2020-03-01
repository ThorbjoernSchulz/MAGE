#include <src/cpu.h>
#include <assert.h>
#include "video.h"
#include "iterator.h"

const uint16_t decoder[] = {
    0x0, 0x1, 0x4, 0x5, 0x10, 0x11, 0x14, 0x15,
    0x40, 0x41, 0x44, 0x45, 0x50, 0x51, 0x54, 0x55,
    0x100, 0x101, 0x104, 0x105, 0x110, 0x111, 0x114, 0x115,
    0x140, 0x141, 0x144, 0x145, 0x150, 0x151, 0x154, 0x155,
    0x400, 0x401, 0x404, 0x405, 0x410, 0x411, 0x414, 0x415,
    0x440, 0x441, 0x444, 0x445, 0x450, 0x451, 0x454, 0x455,
    0x500, 0x501, 0x504, 0x505, 0x510, 0x511, 0x514, 0x515,
    0x540, 0x541, 0x544, 0x545, 0x550, 0x551, 0x554, 0x555,
    0x1000, 0x1001, 0x1004, 0x1005, 0x1010, 0x1011, 0x1014, 0x1015,
    0x1040, 0x1041, 0x1044, 0x1045, 0x1050, 0x1051, 0x1054, 0x1055,
    0x1100, 0x1101, 0x1104, 0x1105, 0x1110, 0x1111, 0x1114, 0x1115,
    0x1140, 0x1141, 0x1144, 0x1145, 0x1150, 0x1151, 0x1154, 0x1155,
    0x1400, 0x1401, 0x1404, 0x1405, 0x1410, 0x1411, 0x1414, 0x1415,
    0x1440, 0x1441, 0x1444, 0x1445, 0x1450, 0x1451, 0x1454, 0x1455,
    0x1500, 0x1501, 0x1504, 0x1505, 0x1510, 0x1511, 0x1514, 0x1515,
    0x1540, 0x1541, 0x1544, 0x1545, 0x1550, 0x1551, 0x1554, 0x1555,
    0x4000, 0x4001, 0x4004, 0x4005, 0x4010, 0x4011, 0x4014, 0x4015,
    0x4040, 0x4041, 0x4044, 0x4045, 0x4050, 0x4051, 0x4054, 0x4055,
    0x4100, 0x4101, 0x4104, 0x4105, 0x4110, 0x4111, 0x4114, 0x4115,
    0x4140, 0x4141, 0x4144, 0x4145, 0x4150, 0x4151, 0x4154, 0x4155,
    0x4400, 0x4401, 0x4404, 0x4405, 0x4410, 0x4411, 0x4414, 0x4415,
    0x4440, 0x4441, 0x4444, 0x4445, 0x4450, 0x4451, 0x4454, 0x4455,
    0x4500, 0x4501, 0x4504, 0x4505, 0x4510, 0x4511, 0x4514, 0x4515,
    0x4540, 0x4541, 0x4544, 0x4545, 0x4550, 0x4551, 0x4554, 0x4555,
    0x5000, 0x5001, 0x5004, 0x5005, 0x5010, 0x5011, 0x5014, 0x5015,
    0x5040, 0x5041, 0x5044, 0x5045, 0x5050, 0x5051, 0x5054, 0x5055,
    0x5100, 0x5101, 0x5104, 0x5105, 0x5110, 0x5111, 0x5114, 0x5115,
    0x5140, 0x5141, 0x5144, 0x5145, 0x5150, 0x5151, 0x5154, 0x5155,
    0x5400, 0x5401, 0x5404, 0x5405, 0x5410, 0x5411, 0x5414, 0x5415,
    0x5440, 0x5441, 0x5444, 0x5445, 0x5450, 0x5451, 0x5454, 0x5455,
    0x5500, 0x5501, 0x5504, 0x5505, 0x5510, 0x5511, 0x5514, 0x5515,
    0x5540, 0x5541, 0x5544, 0x5545, 0x5550, 0x5551, 0x5554, 0x5555,
};

#define get_pixel(tile_line, n) (((tile_line) >> (14 - n * 2)) & 3)
#define flip_line_8bit(line) (((line) & 3) << 6) | (((line) &  12) << 2)\
                                                 | (((line) &  48) >> 2)\
                                                 | (((line) & 192) >> 6)

uint16_t flip_line(uint16_t line) {
  int part_one = (line >> 8);
  int part_two = (line & 0xFF);
  part_one = flip_line_8bit(part_one);
  part_two = flip_line_8bit(part_two);
  return (uint16_t) (part_one | (part_two << 8));
}

static uint8_t get_tile_id_from_pixel(uint8_t *display, int px_x, int px_y);

static uint16_t decode_tile_line(px_data_t *tile, int line);

static void next_screen_pixel(camera_iterator_t *it);

uint32_t next_pixel(camera_iterator_t *it, find_tile_t find_tile,
                    uint32_t *palette) {
  int display_pixel_x, display_pixel_y;
  uint8_t *display;

  /* window or not the window? this is the question.. */
  if (it->window && it->screen_pixel_y >= it->window_y
      && it->screen_pixel_x >= it->window_x - 8) {
    display_pixel_x = it->screen_pixel_x;
    display_pixel_y = it->screen_pixel_y - it->window_y;
    display = it->window;
  } else {
    display_pixel_x = (it->screen_pixel_x + it->scroll_x) & 255;
    display_pixel_y = (it->screen_pixel_y + it->scroll_y) & 255;
    display = it->display;
  }

  uint8_t tile_id = get_tile_id_from_pixel(display,
                                           display_pixel_x,
                                           display_pixel_y);
  px_data_t *current_tile = find_tile(tile_id, it->tiles);

  int tile_row = display_pixel_y & 7;
  int tile_col = display_pixel_x & 7;

  uint16_t tile_line = decode_tile_line(current_tile, tile_row);

  int color_index = get_pixel(tile_line, tile_col);
  uint32_t pixel = palette[color_index];

  next_screen_pixel(it);

  return pixel;
}

/* returns the id of the tile, the iterator points to */
static uint8_t get_tile_id_from_pixel(uint8_t *display, int px_x, int px_y) {
  int x = px_x >> 3;
  int y = px_y >> 3;
  return *(display + y * 32 + x);
}

px_data_t *find_tile_unsigned(uint8_t tile_id, px_data_t *tiles) {
  return tiles + tile_id;
}

px_data_t *find_tile_signed(uint8_t tile_id, px_data_t *tiles) {
  return tiles + 128 + (int8_t) tile_id;
}

static uint16_t decode_tile_line(px_data_t *tile, int line) {
  return (decoder[tile->data[2 * line]] << 1) |
         decoder[tile->data[2 * line + 1]];
}

static void next_screen_pixel(camera_iterator_t *it) {
  if (++it->screen_pixel_x >= 160) {
    it->screen_pixel_x = 0;
    ++it->screen_pixel_y;
  }
}

camera_iterator_t reset_iterator(lcd_regs_t *regs, uint8_t *vram) {
  assert(vram);

  /* calculate where the tile ids are stored */
  gb_address_t display_start = get_bg_display_start_offset(regs);
  uint8_t *display = vram + display_start;

  gb_address_t window_start = get_window_display_start_offset(regs);
  uint8_t *window = vram + window_start;

  /* locate the tile pixel data */
  px_data_t *tiles = (px_data_t *) (vram + (get_bg_data_start_offset(regs)));

  camera_iterator_t it = {
      .scroll_x = regs->scroll_x,
      .scroll_y = regs->scroll_y,
      .window_x = regs->window_x,
      .window_y = regs->window_y,
      .tiles = tiles,
      .display = display,
  };

  if (window_enabled(regs)) it.window = window;

  return it;
}

void update_scrolling(camera_iterator_t *it, lcd_regs_t *regs) {
  /* update scroll-values */
  it->scroll_y = regs->scroll_y;
  it->scroll_x = regs->scroll_x;
}

int next_sprite_pixel(sprite_px_it_t *it, bool x_flip, bool y_flip) {
  if (it->x == 8) {
    it->x = 0;

    /* little bit optimized: The y value increases or decreases depending on
     * the value of y_flip */
    it->y += (int) !y_flip - y_flip;
    if (it->y == ((!y_flip) << 3)) {
      it->y = (y_flip << 3) - y_flip;
      ++it->tile;
    }
  }

  uint16_t line = decode_tile_line(it->tile, it->y);
  if (x_flip) { line = flip_line(line); }
  int color = get_pixel(line, it->x);
  ++it->x;

  return color;
}

