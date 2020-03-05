#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "timer.h"
#include "interrupts.h"

typedef struct debugger debugger_t;

typedef struct mmu_t mmu_t;
typedef struct pixel_processing_unit ppu_t;
typedef uint16_t gb_address_t;

typedef struct cpu {
  /* accumulator */
  uint8_t A;

  /* flags: [ Z | S | HC | C | 0 | 0 | 0 | 0 */
  uint8_t F;

  /* general purpose registers */
  uint8_t B;
  uint8_t C;
  uint8_t D;
  uint8_t E;
  uint8_t H;
  uint8_t L;

  /* stack pointer */
  uint8_t S, P;
  /* program counter */
  gb_address_t pc;

  /* memory management unit */
  mmu_t *mmu;

  ppu_t *ppu;

  bool ei_instruction_used;
  bool interrupts_enabled;
  bool halted;

  cpu_timer_t timer;
  interrupt_controller_t interrupt_controller;
} cpu_t;

/* initializes cpu that is allocated on the stack */
void cpu_init(cpu_t *this, mmu_t *mmu, ppu_t *lcd);

void cpu_delete(cpu_t *cpu);

uint8_t cpu_read(cpu_t *cpu, gb_address_t address);

uint8_t cpu_fetch(cpu_t *cpu);

void cpu_write(cpu_t *cpu, gb_address_t address, uint8_t value);

uint8_t update_cpu_state(cpu_t *cpu, debugger_t *debugger);

gb_address_t concat_bytes(uint8_t reg1, uint8_t reg2);

bool ppu_update(ppu_t *ppu, uint8_t cycles);

void enable_boot_rom(mmu_t *mmu);

void load_boot_rom(FILE *stream);

void __test_write(cpu_t *cpu, gb_address_t address, uint8_t value);

