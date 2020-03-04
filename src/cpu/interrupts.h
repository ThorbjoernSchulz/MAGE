#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <src/memory/memory_handler.h>

typedef struct cpu cpu_t;
typedef struct mmu_t mmu_t;
typedef struct interrupt_registers interrupt_regs_t;
typedef struct interrupt_controller interrupt_controller_t;

#define INT_VBLANK    0x1
#define INT_LCD_STAT  0x2
#define INT_TIMER     0x4
#define INT_SERIAL    0x8
#define INT_JOYPAD    0x10

typedef struct interrupt_memory_handler {
  mem_handler_t base;
  interrupt_controller_t *controller;
} interrupt_mem_handler_t;

typedef struct interrupt_controller {
  cpu_t *cpu;
  interrupt_regs_t *registers;
  interrupt_mem_handler_t handler;
} interrupt_controller_t;

void interrupt_controller_init(interrupt_controller_t *, cpu_t *, mmu_t *);
void raise_interrupt(cpu_t *cpu, uint8_t interrupt);

uint8_t process_interrupts(cpu_t *cpu);

bool interrupts_ready(cpu_t *cpu);
