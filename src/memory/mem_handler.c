#include "memory_handler.h"
#include <stdlib.h>

DEF_MEM_DESTROY(mem_handler_default_destroy) {
  free(this);
}

DEF_MEM_DESTROY(mem_handler_stack_destroy) { /* nothing to do */ }

mem_handler_t *
mem_handler_create_d(mem_read_t read, mem_write_t write, destructor_t d) {
  mem_handler_t *handler = calloc(1, sizeof(mem_handler_t));
  if (!handler) return 0;

  handler->read = read;
  handler->write = write;
  handler->destroy = d;

  return handler;
}

mem_handler_t *mem_handler_create(mem_read_t read, mem_write_t write) {
  return mem_handler_create_d(read, write, mem_handler_default_destroy);
}

