#include "interrupts.h"
#include "src/cpu/cpu.h"
#include "src/memory/mmu.h"
#include "instructions.h"

extern void die(const char *s);

typedef struct interrupt_registers {
  uint8_t flags;
  uint8_t NA[239];
  uint8_t enable;
} interrupt_regs_t;

DEF_MEM_READ(interrupt_read) {
  interrupt_mem_handler_t *handler = (interrupt_mem_handler_t *) this;
  uint8_t *registers = (uint8_t *) handler->controller->registers;
  uint8_t byte = registers[address - 0xFF0F];

  if (address == 0xFF0F) {
    //byte |= 0xE0;
  }

  return byte;
}

DEF_MEM_WRITE(interrupt_write) {
  interrupt_mem_handler_t *handler = (interrupt_mem_handler_t *) this;
  uint8_t *registers = (uint8_t *) handler->controller->registers;
  registers[address - 0xFF0F] = value;
}

void interrupt_controller_init(interrupt_controller_t *this, cpu_t *cpu,
                               mmu_t *mmu) {
  this->handler.base.read = interrupt_read;
  this->handler.base.write = interrupt_write;
  this->handler.base.destroy = mem_handler_stack_destroy;
  this->handler.controller = this;

  mem_tuple_t tuple = mmu_map_register(mmu, 0xFF0F);
  mmu_register_mem_handler(mmu, (mem_handler_t *) &this->handler, tuple.handle);
  this->registers = (interrupt_regs_t *) tuple.memory;

  tuple = mmu_map_register(mmu, 0xFFFF);
  mmu_register_mem_handler(mmu, (mem_handler_t *) &this->handler, tuple.handle);

  this->cpu = cpu;
}

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
  if (cpu->halted) {
    ++cpu->pc;
    cpu->halted = false;
  }
  push(cpu, high_byte(cpu->pc), low_byte(cpu->pc));

  call_interrupt_handler(cpu, interrupt);
}

bool interrupts_ready(cpu_t *cpu) {
  interrupt_regs_t *registers = cpu->interrupt_controller.registers;
  return (registers->enable & registers->flags & 0x1F) != 0;
}

uint8_t process_interrupts(cpu_t *cpu) {
  /* handle halt behaviour */
  if (!cpu->interrupts_enabled) {
    if (cpu->halted && interrupts_ready(cpu))
      cpu->halted = false;
    return 0;
  }

  interrupt_regs_t *registers = cpu->interrupt_controller.registers;

  uint8_t if_reg = registers->flags;
  if (!if_reg)
    return 0;

  uint8_t priority_interrupt = find_highest_bit(if_reg);
  /* clear flag */
  if_reg &= ~(priority_interrupt);
  registers->flags = if_reg;

  uint8_t enabled_interrupts = registers->enable;
  if (!(enabled_interrupts & priority_interrupt))
    return 0;

  execute_interrupt(cpu, priority_interrupt);
  return 20;
}

void raise_interrupt(cpu_t *cpu, uint8_t interrupt) {
  cpu->interrupt_controller.registers->flags |= interrupt;
}
