#pragma once
#include "memory_handler.h"

typedef struct cartridge_t cartridge_t;

cartridge_t *cartridge_new(const char *filename);
void cartridge_delete(cartridge_t *);

mem_handler_t *cartridge_get_memory_handler(cartridge_t *cart);
void cartridge_save_game(cartridge_t *cart);

