#pragma once
#include <stdint.h>
#include <memory/memory_handler.h>

typedef struct cpu cpu_t;
typedef struct mmu_t mmu_t;
typedef struct timer cpu_timer_t;
typedef struct timer_registers timer_regs_t;

typedef struct timer_memory_handler {
  mem_handler_t base;
  cpu_timer_t *timer;
} timer_mem_handler_t;

typedef struct timer {
  cpu_t *interrupt_line;
  timer_regs_t *registers;
  uint32_t clock;
  uint32_t div_clock;
  timer_mem_handler_t handler;
} cpu_timer_t;

void timer_update(cpu_timer_t *this, uint8_t cycles);
void timer_init(cpu_timer_t *this, cpu_t *cpu, mmu_t *mmu);
void div_update(cpu_timer_t *this, uint8_t cycles);
