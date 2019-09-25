#pragma once

#include <stdint.h>

/*
 * With the game boy, reads and writes have different effects
 * dependent on their target address. A write to read-only memory
 * may not lead to a crash but to very specific behaviour.
 * To cope with this in this project, a simple paging like mechanism
 * is used:
 *
 * One can request for a certain address range to be mapped to
 * a specific "memory handler". When a read or a write happens
 * on any address in that range, the memory handler gets called.
 *
 * To do this simply request an as_handle (as in address space)
 * with mmu_map_memory, and call mmu_register_mem_handler with it.
 *
 * A memory handler can be defined by implementing the interface below.
 * Look into others for examples.
 */

typedef uint16_t gb_address_t;

typedef struct memory_handler mem_handler_t;

typedef uint8_t (*mem_read_t)(mem_handler_t *, gb_address_t);

typedef void (*mem_write_t)(mem_handler_t *, gb_address_t, uint8_t);

typedef void (*destructor_t)(mem_handler_t *);

#define DEF_MEM_DESTROY(name) void name(mem_handler_t *this)
#define DEF_MEM_READ(name)  uint8_t name(mem_handler_t *this, \
                                         gb_address_t address)
#define DEF_MEM_WRITE(name)  void name(mem_handler_t *this, \
                                       gb_address_t address, uint8_t value)\

struct memory_handler {
  mem_read_t read;
  mem_write_t write;
  destructor_t destroy;
};

mem_handler_t *mem_handler_create(mem_read_t read, mem_write_t write);

mem_handler_t *mem_handler_create_d(mem_read_t read, mem_write_t write,
                                    destructor_t d);

DEF_MEM_DESTROY(mem_handler_default_destroy);

DEF_MEM_DESTROY(mem_handler_stack_destroy);
