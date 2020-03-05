#include "cpu.h"
#include <memory/mmu.h>
#include "interrupts.h"

extern void mmu_write(mmu_t *mmu, gb_address_t address, uint8_t value);

extern uint8_t mmu_read(mmu_t *mmu, gb_address_t address);

void cpu_init(cpu_t *this, mmu_t *mmu, ppu_t *lcd) {
  this->mmu = mmu;
  this->ppu = lcd;
  timer_init(&this->timer, this, mmu);
  interrupt_controller_init(&this->interrupt_controller, this, mmu);
}

void cpu_delete(cpu_t *cpu) {
  mmu_delete(cpu->mmu);
  ppu_delete(cpu->ppu);
}

uint8_t cpu_read(cpu_t *cpu, gb_address_t address) {
  return mmu_read(cpu->mmu, address);
}

uint8_t cpu_fetch(cpu_t *cpu) {
  return cpu_read(cpu, ++cpu->pc);
}

void cpu_write(cpu_t *cpu, gb_address_t address, uint8_t value) {
  mmu_write(cpu->mmu, address, value);
}

void __test_write(cpu_t *cpu, gb_address_t address, uint8_t value) {
  mmu_write(cpu->mmu, address, value);
}

gb_address_t concat_bytes(uint8_t reg1, uint8_t reg2) {
  return (uint16_t) reg1 << 8 | (uint16_t) reg2;
}

