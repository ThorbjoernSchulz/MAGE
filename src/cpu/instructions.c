#include <stdio.h>

#include "instructions.h"

#define half_carry16(a, b) (((a) & 0xFFF) + ((b) & 0xFFF)) > 0xFFF
#define half_carry_c(a, b, c) (((a) & 0xF) + ((b) & 0xF) + (c)) > 0xF

uint8_t inc(cpu_t *cpu, uint8_t value) {
  cpu->F &= ~(FLAG_ZERO | FLAG_HCARRY);
  set_add_flag(cpu);
  if (half_carry(value, 1)) set_hcarry_flag(cpu);
  ++value;
  if (!value) set_zero_flag(cpu);
  return value;
}

void indirect_inc(cpu_t *cpu, gb_address_t address) {
  bool carry_was_set = carry_set(cpu);
  clear_flags(cpu);

  uint8_t byte = cpu_read(cpu, address);

  if ((byte & 0xF) == 0xF) set_hcarry_flag(cpu);

  ++byte;

  if (!byte) set_zero_flag(cpu);

  cpu_write(cpu, address, byte);
  if (carry_was_set) set_carry_flag(cpu);
}

uint8_t dec(cpu_t *cpu, uint8_t value) {
  cpu->F &= ~(FLAG_ZERO | FLAG_HCARRY);
  set_sub_flag(cpu);
  if (!(value & 0xF)) set_hcarry_flag(cpu);
  --value;
  if (!value) set_zero_flag(cpu);
  return value;
}

void indirect_dec(cpu_t *cpu, gb_address_t address) {
  bool carry_was_set = carry_set(cpu);
  clear_flags(cpu);
  set_sub_flag(cpu);

  uint8_t byte = cpu_read(cpu, address);

  if ((byte & 0xF) == 0) set_hcarry_flag(cpu);

  --byte;

  if (!byte) set_zero_flag(cpu);

  cpu_write(cpu, address, byte);
  if (carry_was_set) set_carry_flag(cpu);
}

uint16_t add16(cpu_t *cpu, uint8_t h1, uint8_t l1, uint8_t h2, uint8_t l2) {
  cpu->F &= ~(FLAG_HCARRY | FLAG_CARRY | FLAG_SUB);
  uint16_t operand1 = concat_bytes(h1, l1);
  uint16_t operand2 = concat_bytes(h2, l2);
  uint32_t result = operand1 + operand2;

  if (half_carry16(operand1, operand2)) set_hcarry_flag(cpu);
  if (result & 0xFFFF0000) set_carry_flag(cpu);

  operand1 = (uint16_t) (result & 0x0000FFFF);
  return operand1;
}

uint16_t add16s(cpu_t *cpu, uint8_t h, uint8_t l, int8_t r) {
  clear_flags(cpu);

  uint16_t operand1 = concat_bytes(h, l);
  uint16_t result = operand1 + r;

  if ((result & 0xFF) < (operand1 & 0xFF)) set_carry_flag(cpu);
  if ((result & 0xF) < (operand1 & 0xF)) set_hcarry_flag(cpu);

  return result;
}

uint8_t add8(cpu_t *cpu, uint8_t reg) {
  clear_flags(cpu);
  uint16_t sum = cpu->A + reg;

  if (half_carry(cpu->A, reg)) set_hcarry_flag(cpu);
  if (sum & 0xFF00) set_carry_flag(cpu);

  uint8_t result = (uint8_t) (sum & 0xFF);
  if (!result) set_zero_flag(cpu);
  return result;
}

uint8_t adc8(cpu_t *cpu, uint8_t reg) {
  bool carry_was_set = carry_set(cpu);
  clear_flags(cpu);

  uint16_t sum = cpu->A + reg + carry_was_set;

  if (half_carry_c(cpu->A, reg, carry_was_set)) set_hcarry_flag(cpu);
  if (sum & 0xFF00) set_carry_flag(cpu);

  uint8_t result = (uint8_t) (sum & 0xFF);
  if (!result) set_zero_flag(cpu);
  return result;
}

uint8_t sub8(cpu_t *cpu, uint8_t reg) {
  clear_flags(cpu);
  set_sub_flag(cpu);

  if (cpu->A == reg) set_zero_flag(cpu);

  if ((cpu->A & 0xF) < (reg & 0xF)) set_hcarry_flag(cpu);

  if (cpu->A < reg) set_carry_flag(cpu);

  return cpu->A - reg;
}

uint8_t sbc8(cpu_t *cpu, uint8_t reg) {
  int A = cpu->A;
  int value = reg;

  A -= value;
  if (carry_set(cpu)) A -= 1;

  clear_flags(cpu);
  set_sub_flag(cpu);

  if (A < 0) set_carry_flag(cpu);

  A &= 0xFF;

  if (((A ^ value ^ cpu->A) & 0x10) == 0x10)
    set_hcarry_flag(cpu);

  if (A == 0) set_zero_flag(cpu);

  return (uint8_t) (A);
}

uint8_t and8(cpu_t *cpu, uint8_t reg) {
  clear_flags(cpu);
  set_hcarry_flag(cpu);
  uint8_t result = cpu->A & reg;
  if (!result) set_zero_flag(cpu);
  return result;
}

uint8_t or8(cpu_t *cpu, uint8_t reg) {
  clear_flags(cpu);
  uint8_t result = cpu->A | reg;
  if (!result) set_zero_flag(cpu);
  return result;
}

uint8_t xor8(cpu_t *cpu, uint8_t reg) {
  clear_flags(cpu);
  uint8_t result = cpu->A ^reg;
  if (!result) set_zero_flag(cpu);
  return result;
}

void push(cpu_t *cpu, uint8_t high, uint8_t low) {
  gb_address_t sp = concat_bytes(cpu->S, cpu->P);
  cpu_write(cpu, --sp, high);
  cpu_write(cpu, --sp, low);
  cpu->S = high_byte(sp);
  cpu->P = low_byte(sp);
}

uint16_t pop(cpu_t *cpu) {
  gb_address_t sp = concat_bytes(cpu->S, cpu->P);
  uint8_t low = cpu_read(cpu, sp++);
  uint8_t high = cpu_read(cpu, sp++);
  cpu->S = high_byte(sp);
  cpu->P = low_byte(sp);
  return concat_bytes(high, low);
}

/* because the pc gets incremented after each instruction,
 * we need to jump to address - 1 */
void jump(cpu_t *cpu, gb_address_t address) {
  cpu->pc = --address;
}

static void rlc(cpu_t *cpu, uint8_t *target) {
  clear_flags(cpu);
  uint8_t reg = *target;

  bool carry = (reg & 0x80) != 0;
  reg <<= 1;
  reg |= carry;

  if (carry) set_carry_flag(cpu);
  if (!reg) set_zero_flag(cpu);

  *target = reg;
}

void rlca(cpu_t *cpu) {
  rlc(cpu, &cpu->A);
  unset_zero_flag(cpu);
}

static void rrc(cpu_t *cpu, uint8_t *target) {
  clear_flags(cpu);
  uint8_t reg = *target;

  uint8_t carry = (uint8_t) (reg & 0x1) << 7;
  reg >>= 1;
  reg |= carry;

  if (carry) set_carry_flag(cpu);
  if (!reg) set_zero_flag(cpu);

  *target = reg;
}

void rrca(cpu_t *cpu) {
  rrc(cpu, &cpu->A);
  unset_zero_flag(cpu);
}

static void rl(cpu_t *cpu, uint8_t *target) {
  bool carry_was_set = carry_set(cpu);
  clear_flags(cpu);
  uint8_t reg = *target;

  bool carry = (reg & 0x80) != 0;
  reg <<= 1;
  reg |= carry_was_set;

  if (carry) set_carry_flag(cpu);
  if (!reg) set_zero_flag(cpu);

  *target = reg;
}

void rla(cpu_t *cpu) {
  rl(cpu, &cpu->A);
  unset_zero_flag(cpu);
}

static void rr(cpu_t *cpu, uint8_t *target) {
  bool carry_was_set = carry_set(cpu);
  clear_flags(cpu);

  uint8_t reg = *target;

  uint8_t carry = (uint8_t) (reg & 0x1);
  reg >>= 1;
  reg |= carry_was_set << 7;

  if (carry) set_carry_flag(cpu);
  if (!reg) set_zero_flag(cpu);

  *target = reg;
}

void rra(cpu_t *cpu) {
  rr(cpu, &cpu->A);
  unset_zero_flag(cpu);
}

static void sla(cpu_t *cpu, uint8_t *target) {
  clear_flags(cpu);
  uint8_t reg = *target;

  if (reg & 0x80) set_carry_flag(cpu);
  reg <<= 1;

  if (!reg) set_zero_flag(cpu);
  *target = reg;
}

static void sra(cpu_t *cpu, uint8_t *target) {
  clear_flags(cpu);
  uint8_t reg = *target;

  uint8_t msb = reg & (uint8_t) 0x80;
  if (reg & 0x1) set_carry_flag(cpu);
  reg >>= 1;
  reg |= msb;

  if (!reg) set_zero_flag(cpu);
  *target = reg;
}

static void swap(cpu_t *cpu, uint8_t *target) {
  clear_flags(cpu);
  uint8_t reg = *target;
  reg = ((reg & (uint8_t) 0xF0) >> 4) | ((reg & (uint8_t) 0xF) << 4);
  if (!reg) set_zero_flag(cpu);
  *target = reg;
}

static void srl(cpu_t *cpu, uint8_t *target) {
  clear_flags(cpu);
  uint8_t reg = *target;

  if (reg & 0x1) set_carry_flag(cpu);
  reg >>= 1;

  if (!reg) set_zero_flag(cpu);
  *target = reg;
}

void daa(cpu_t *cpu) {
  uint8_t correction = 0;
  uint8_t flags = 0;

  if (hcarry_set(cpu) || (add_set(cpu) && (cpu->A & 0xF) > 9)) {
    correction |= 0x6;
  }
  if (carry_set(cpu) || (add_set(cpu) && cpu->A > 0x99)) {
    correction |= 0x60;
    flags |= FLAG_CARRY;
  }

  cpu->A += sub_set(cpu) ? -correction : correction;

  if (!cpu->A) flags |= FLAG_ZERO;

  unset_zero_flag(cpu);
  unset_carry_flag(cpu);
  unset_hcarry_flag(cpu);
  cpu->F |= flags;
}

static void bitn(cpu_t *cpu, uint8_t bit, uint8_t target) {
  set_add_flag(cpu);
  set_hcarry_flag(cpu);
  if (target & (1 << bit)) unset_zero_flag(cpu);
  else
    set_zero_flag(cpu);
}

static void bit0(cpu_t *cpu, uint8_t *target) {
  bitn(cpu, 0, *target);
}

static void bit1(cpu_t *cpu, uint8_t *target) {
  bitn(cpu, 1, *target);
}

static void bit2(cpu_t *cpu, uint8_t *target) {
  bitn(cpu, 2, *target);
}

static void bit3(cpu_t *cpu, uint8_t *target) {
  bitn(cpu, 3, *target);
}

static void bit4(cpu_t *cpu, uint8_t *target) {
  bitn(cpu, 4, *target);
}

static void bit5(cpu_t *cpu, uint8_t *target) {
  bitn(cpu, 5, *target);
}

static void bit6(cpu_t *cpu, uint8_t *target) {
  bitn(cpu, 6, *target);
}

static void bit7(cpu_t *cpu, uint8_t *target) {
  bitn(cpu, 7, *target);
}

#define reset_bit(a, n) (a) &= ~(1 << (n))

static void res0(cpu_t *cpu, uint8_t *target) {
  reset_bit(*target, 0);
}

static void res1(cpu_t *cpu, uint8_t *target) {
  reset_bit(*target, 1);
}

static void res2(cpu_t *cpu, uint8_t *target) {
  reset_bit(*target, 2);
}

static void res3(cpu_t *cpu, uint8_t *target) {
  reset_bit(*target, 3);
}

static void res4(cpu_t *cpu, uint8_t *target) {
  reset_bit(*target, 4);
}

static void res5(cpu_t *cpu, uint8_t *target) {
  reset_bit(*target, 5);
}

static void res6(cpu_t *cpu, uint8_t *target) {
  reset_bit(*target, 6);
}

static void res7(cpu_t *cpu, uint8_t *target) {
  reset_bit(*target, 7);
}

#define set_bit(a, n) (a) |= (1 << (n))

static void set0(cpu_t *cpu, uint8_t *target) {
  set_bit(*target, 0);
}

static void set1(cpu_t *cpu, uint8_t *target) {
  set_bit(*target, 1);
}

static void set2(cpu_t *cpu, uint8_t *target) {
  set_bit(*target, 2);
}

static void set3(cpu_t *cpu, uint8_t *target) {
  set_bit(*target, 3);
}

static void set4(cpu_t *cpu, uint8_t *target) {
  set_bit(*target, 4);
}

static void set5(cpu_t *cpu, uint8_t *target) {
  set_bit(*target, 5);
}

static void set6(cpu_t *cpu, uint8_t *target) {
  set_bit(*target, 6);
}

static void set7(cpu_t *cpu, uint8_t *target) {
  set_bit(*target, 7);
}

typedef struct cb_instruction {
  void (*instruction)(cpu_t *cpu, uint8_t *target);

  uint8_t *target;
  uint8_t clocks;
} cb_inst_t;

static cb_inst_t cb_decoder(cpu_t *cpu, uint8_t byte) {
  static void (*instructions[])(cpu_t *, uint8_t *) = {
      rlc, rrc, rl, rr, sla, sra, swap, srl,
      bit0, bit1, bit2, bit3, bit4, bit5, bit6, bit7,
      res0, res1, res2, res3, res4, res5, res6, res7,
      set0, set1, set2, set3, set4, set5, set6, set7
  };

  uint8_t instr_id = byte >> 3;
  uint8_t register_ = byte & (uint8_t) 7;

  cb_inst_t instruction = {.instruction = instructions[instr_id]};

  instruction.clocks = 8;

  switch (register_) {
    case 0:
      instruction.target = &cpu->B;
      break;
    case 1:
      instruction.target = &cpu->C;
      break;
    case 2:
      instruction.target = &cpu->D;
      break;
    case 3:
      instruction.target = &cpu->E;
      break;
    case 4:
      instruction.target = &cpu->H;
      break;
    case 5:
      instruction.target = &cpu->L;
      break;
    case 6:
      /* (HL) */
      instruction.target = NULL;
      instruction.clocks += (instr_id > 7 && instr_id < 16) ? 4 : 8;
      break;
    case 7:
      instruction.target = &cpu->A;
      break;

    default:
      break;
  }

  return instruction;
}

uint8_t cb_inst_execute(cpu_t *cpu, uint8_t byte) {
  cb_inst_t instruction = cb_decoder(cpu, byte);

  if (!instruction.target) {
    /* (HL) */
    gb_address_t address = concat_bytes(cpu->H, cpu->L);
    uint8_t target = cpu_read(cpu, address);
    instruction.instruction(cpu, &target);
    cpu_write(cpu, address, target);

    return instruction.clocks;
  }

  instruction.instruction(cpu, instruction.target);
  return instruction.clocks;
}
