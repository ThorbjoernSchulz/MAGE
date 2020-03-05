#include <stdlib.h>

#include "input.h"
#include <cpu/interrupts.h>
#include <memory/memory_handler.h>
#include <memory/mmu.h>

struct input_t;

typedef struct input_mem_handler {
  mem_handler_t base;
  struct input_t *input;
} input_mem_handler_t ;

typedef struct input_t {
  cpu_t *interrupt_line;
  uint8_t *register_;
  uint8_t control_pad_state;
  input_mem_handler_t memory_handler;
} input_t;

void input_press(input_t *input, uint8_t key) {
  input->control_pad_state |= key;
  raise_interrupt(input->interrupt_line, INT_JOYPAD);
}

void input_unpress(input_t *input, uint8_t key) {
  input->control_pad_state &= ~key;
  *input->register_ = 0xFF;
}

DEF_MEM_READ(input_read) {
  input_mem_handler_t *handler = (input_mem_handler_t *)this;
  input_t *input = handler->input;

  uint8_t register_ = *input->register_;
  /* reading joypad input depends on the bits written to this
   * address previously, specifically the 4th and 5th bit,
   * which decide whether a button or direction key was queried */
  if ((register_ & 0x20) == 0)
    return ~(input->control_pad_state >> 4);

  if ((register_ & 0x10) == 0)
    return ~(input->control_pad_state & 0x0F);

  return register_;
}

DEF_MEM_WRITE(input_write) {
  input_mem_handler_t *handler = (input_mem_handler_t *)this;
  input_t *input = handler->input;

  /* the lower 4 bits are read only */
  uint8_t current = *input->register_;
  current &= 0x0F;
  current |= value & 0xF0;
  *input->register_ = current;
}

input_t *input_new(cpu_t *interrupt_line, mmu_t *mmu) {
  input_t *input = calloc(1, sizeof(input_t));
  if (!input) return 0;

  input->interrupt_line = interrupt_line;

  input->memory_handler.base.read = input_read;
  input->memory_handler.base.write = input_write;
  input->memory_handler.base.destroy = mem_handler_stack_destroy;
  input->memory_handler.input = input;

  mem_tuple_t tuple = mmu_map_register(mmu, 0xFF00);
  mmu_register_mem_handler(mmu, (mem_handler_t *)&input->memory_handler, tuple.handle);
  input->register_ = tuple.memory;
  return input;
}

void input_delete(input_t *input) {
  free(input);
}

