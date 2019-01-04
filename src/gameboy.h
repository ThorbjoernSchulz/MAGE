#pragma once
#include <stdbool.h>

struct game_boy_t;
typedef struct game_boy_t * gb_t;

typedef struct SDL_Surface SDL_Surface;
typedef struct SDL_Window SDL_Window;

gb_t game_boy_new(const char *boot_file, SDL_Surface *surface);
void game_boy_delete(gb_t gb);

void game_boy_insert_game(gb_t gb, const char *game_path);
void game_boy_run(gb_t gb, SDL_Surface *screen_data, SDL_Window *window);

void game_boy_entry_after_boot(gb_t gb);
