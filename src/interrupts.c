#include "interrupts.h"
#include "cpu.h"
#include "mmu.h"
#include "instructions.h"

extern void die(const char *s);

static uint8_t find_highest_bit(uint8_t byte) {
  if (!byte) return 0;

  uint8_t ret = 1;
  while (byte >>= 1) ret <<= 1;

  return ret;
}

static void call_interrupt_handler(cpu_t *cpu, uint8_t interrupt) {
  gb_address_t jump_address = 0;
  switch (interrupt) {
    case INT_VBLANK:
      jump_address = 0x40;
      break;

    case INT_LCD_STAT:
      jump_address = 0x48;
      break;

    case INT_TIMER:
      jump_address = 0x50;
      break;

    case INT_SERIAL:
      jump_address = 0x58;
      break;

    case INT_JOYPAD:
      jump_address = 0x60;
      break;

    default:
      die("Invalid interrupt, this should not happen.");
      break;
  }

  cpu->pc = jump_address;
}

static void execute_interrupt(cpu_t *cpu, uint8_t interrupt) {
  cpu->interrupts_enabled = false;

  /* save program counter (if halted, push the location after the halt) */
  if (cpu->halted) { ++cpu->pc; cpu->halted = false; }
  push(cpu, high_byte(cpu->pc), low_byte(cpu->pc));

  call_interrupt_handler(cpu, interrupt);
}

bool interrupts_ready(cpu_t *cpu) {
  mmu_t *mmu = cpu->mmu;
  return (mmu_get_interrupt_enable_reg(mmu) &
          mmu_get_interrupt_flags_reg(mmu)  &
          0x1F) != 0;
}

uint8_t process_interrupts(cpu_t *cpu) {
  /* handle halt behaviour */
  if (!cpu->interrupts_enabled) {
    if (cpu->halted && interrupts_ready(cpu))
      cpu->halted = false;
    return 0;
  }

  uint8_t if_reg = mmu_get_interrupt_flags_reg(cpu->mmu);
  if (!if_reg)
    return 0;

  uint8_t priority_interrupt = find_highest_bit(if_reg);
  /* clear flag */
  if_reg &= ~(priority_interrupt);
  mmu_set_interrupt_flags_reg(cpu->mmu, if_reg);

  uint8_t enabled_interrupts = mmu_get_interrupt_enable_reg(cpu->mmu);
  if (!(enabled_interrupts & priority_interrupt))
    return 0;

  execute_interrupt(cpu, priority_interrupt);
  return 20;
}

void raise_interrupt(cpu_t *cpu, uint8_t interrupt) {
  uint8_t reg = mmu_get_interrupt_flags_reg(cpu->mmu);
  reg |= interrupt;
  mmu_set_interrupt_flags_reg(cpu->mmu, reg);
}
