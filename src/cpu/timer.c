#include <src/memory/memory_handler.h>
#include "src/cpu/cpu.h"
#include "src/memory/mmu.h"
#include "src/cpu/interrupts.h"
#include "timer.h"

static const uint16_t input_clock_types[] = {
    1024, 16, 64, 256
};

typedef struct timer_registers {
  uint8_t div;
  uint8_t counter;
  uint8_t modulo;
  uint8_t control;
} timer_regs_t;

DEF_MEM_READ(timer_read) {
  timer_mem_handler_t *handler =(timer_mem_handler_t *)this;
  return ((uint8_t *)handler->timer->registers)[address - 0xFF04];
}

DEF_MEM_WRITE(timer_write) {
  timer_mem_handler_t *handler =(timer_mem_handler_t *)this;
  timer_regs_t *regs = handler->timer->registers;

  if (address == 0xFF07) {
    value &= 7;
    if ((regs->control & 3) != (value & 3)) {
      /* reset counter */
      regs->counter = regs->modulo;
      handler->timer->clock = 0;
    }
  }

  ((uint8_t *)handler->timer->registers)[address - 0xFF04] = value;
}

void timer_init(cpu_timer_t *this, cpu_t *cpu, mmu_t *mmu) {
  this->handler.base.read = timer_read;
  this->handler.base.write = timer_write;
  this->handler.base.destroy = mem_handler_stack_destroy;
  this->handler.timer = this;

  mem_tuple_t tuple = mmu_map_memory(mmu, 0xFF04, 0xFF07);
  this->registers = (timer_regs_t *)tuple.memory;

  mmu_register_mem_handler(mmu, (mem_handler_t *)&this->handler, tuple.handle);

  this->interrupt_line = cpu;
}

void timer_update(cpu_timer_t *this, uint8_t cycles) {
  uint8_t reg = this->registers->control;

  uint8_t timer_start = reg & (uint8_t) 4;
  if (!timer_start) return;

  uint32_t timer_clock = this->clock;
  timer_clock += cycles;

  uint8_t input_clock_select = reg & (uint8_t) 3;
  uint16_t type = input_clock_types[input_clock_select];

  while (timer_clock >= type) {
    timer_clock -= type;

    uint8_t tima = this->registers->counter;

    /* overflow */
    if (tima == 0xFF) {
      raise_interrupt(this->interrupt_line, INT_TIMER);
      /* set counter to TMA */
      tima = this->registers->modulo;
    } else
      ++tima;

    this->registers->counter = tima;
  }

  this->clock = timer_clock;
}

void div_update(cpu_timer_t *this, uint8_t cycles) {
  this->div_clock += cycles;

  if (this->div_clock >= 256) {
    this->div_clock -= 256;
    ++this->registers->div;
  }
}