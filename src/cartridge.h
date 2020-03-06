#pragma once

#include <memory/memory_handler.h>

typedef struct cartridge_t cartridge_t;

cartridge_t *cartridge_new(const char *game_path, const char *save_file);

void cartridge_delete(cartridge_t *);

mem_handler_t *cartridge_get_memory_handler(cartridge_t *cart);

