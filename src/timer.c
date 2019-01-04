#include "src/cpu.h"
#include "src/mmu.h"
#include "src/interrupts.h"

static const uint16_t input_clock_types[] = {
    1024, 16, 64, 256
};

void timer_update(cpu_t *cpu, uint8_t cycles) {
  mmu_t *mmu = cpu->mmu;
  uint8_t reg = mmu_get_timer_control_reg(mmu);

  uint8_t timer_start = reg & (uint8_t)4;
  if (!timer_start) return;

  uint32_t timer_clock = mmu_get_timer_clock(mmu);
  timer_clock += cycles;

  uint8_t input_clock_select = reg & (uint8_t)3;
  uint16_t type = input_clock_types[input_clock_select];

  while (timer_clock >= type) {
    timer_clock -= type;

    uint8_t tima = mmu_get_timer_counter_reg(mmu);

    /* overflow */
    if (tima == 0xFF) {
      raise_interrupt(cpu, INT_TIMER);
      /* set counter to TMA */
      tima = mmu_get_timer_modulo_reg(cpu->mmu);
    } else
      ++tima;

    mmu_set_timer_counter_reg(cpu->mmu, tima);
  }

  mmu_set_timer_clock(mmu, timer_clock);
}

void div_update(cpu_t *cpu, uint8_t cycles) {
  cpu->div_clock += cycles;

  if (cpu->div_clock >= 256) {
    cpu->div_clock -= 256;
    mmu_inc_div_reg(cpu->mmu);
  }
}