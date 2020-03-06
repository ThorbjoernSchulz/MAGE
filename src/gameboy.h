#pragma once

#include <stdbool.h>

struct game_boy_t;
typedef struct game_boy_t *gb_t;
typedef struct display display_t;

gb_t game_boy_new(const char *boot_file, display_t *display);

void game_boy_delete(gb_t gb);

void
game_boy_insert_game(gb_t gb, const char *game_path, const char *save_file);

void game_boy_run(gb_t gb);

void game_boy_entry_after_boot(gb_t gb);
