#pragma once

#include "cpu.h"

#define high_byte(n)  (uint8_t)(((n) >> 8) & 0xFF)
#define low_byte(n)   (uint8_t)((n) & 0xFF)

#define FLAG_ZERO   0x80
#define FLAG_SUB    0x40
#define FLAG_HCARRY  0x20
#define FLAG_CARRY  0x10

#define set_zero_flag(cpu) (cpu)->F |= FLAG_ZERO
#define set_sub_flag(cpu) (cpu)->F |= FLAG_SUB
#define set_carry_flag(cpu) (cpu)->F |= FLAG_CARRY
#define set_hcarry_flag(cpu) (cpu)->F |= FLAG_HCARRY
#define unset_zero_flag(cpu) (cpu)->F &= ~(FLAG_ZERO)
#define set_add_flag(cpu) (cpu)->F &= ~(FLAG_SUB)
#define unset_carry_flag(cpu) (cpu)->F &= ~(FLAG_CARRY)
#define unset_hcarry_flag(cpu) (cpu)->F &= ~(FLAG_HCARRY)
#define clear_flags(cpu) (cpu)->F = 0

#define carry_set(cpu) (uint8_t)(((cpu)->F & FLAG_CARRY) != 0)
#define hcarry_set(cpu) (uint8_t)(((cpu)->F & FLAG_HCARRY) != 0)
#define add_set(cpu) (uint8_t)(((cpu)->F & FLAG_SUB) == 0)
#define sub_set(cpu) (uint8_t)(((cpu)->F & FLAG_SUB) != 0)
#define zero_set(cpu) (uint8_t)(((cpu)->F & FLAG_ZERO) != 0)

uint8_t inc(cpu_t *cpu, uint8_t value);

void indirect_inc(cpu_t *cpu, gb_address_t address);

uint8_t dec(cpu_t *cpu, uint8_t value);

void indirect_dec(cpu_t *cpu, gb_address_t address);

uint8_t adc8(cpu_t *cpu, uint8_t reg);

uint8_t add8(cpu_t *cpu, uint8_t reg);

uint16_t add16(cpu_t *cpu, uint8_t h1, uint8_t l1, uint8_t h2, uint8_t l2);

uint16_t add16s(cpu_t *cpu, uint8_t h, uint8_t l, int8_t r);

uint8_t sub8(cpu_t *cpu, uint8_t reg);

uint8_t sbc8(cpu_t *cpu, uint8_t reg);

uint8_t and8(cpu_t *cpu, uint8_t reg);

uint8_t or8(cpu_t *cpu, uint8_t reg);

uint8_t xor8(cpu_t *cpu, uint8_t reg);

void rrca(cpu_t *cpu);

void rra(cpu_t *cpu);

void rlca(cpu_t *cpu);

void rla(cpu_t *cpu);

void daa(cpu_t *cpu);

/* returns the clock cycles taken */
uint8_t cb_inst_execute(cpu_t *cpu, uint8_t byte);

#define half_carry(a, b) (((a) & 0xF) + ((b) & 0xF)) > 0xF

/* +++++++++++++++++++++++++++++++++++++++++++++++
 * +              LOADS AND STORES               +
 * +++++++++++++++++++++++++++++++++++++++++++++++
 */

#define store_short_value(CPU, HIGH, LOW, REG) {      \
  gb_address_t address = concat_bytes((HIGH), (LOW)); \
  cpu_write((CPU), address, (REG)); }                 \

#define load_short_value(CPU, REG, HIGH, LOW) {       \
  gb_address_t address = concat_bytes((HIGH), (LOW)); \
  (REG) = cpu_read((CPU), address); }                 \

#define store_long_value(CPU, HIGH, LOW) {            \
  uint8_t low = cpu_fetch((CPU));                     \
  uint8_t high = cpu_fetch((CPU));                    \
  gb_address_t address = concat_bytes(high, low);     \
  cpu_write((CPU), address, (LOW));                   \
  cpu_write((CPU), ++address, (HIGH));}               \

#define load_long_value(CPU, HIGH, LOW) {             \
  (LOW) = cpu_fetch((CPU));                           \
  (HIGH) = cpu_fetch((CPU));}                         \

/* +++++++++++++++++++++++++++++++++++++++++++++++
 * +              ARITHMETIC                     +
 * +++++++++++++++++++++++++++++++++++++++++++++++
 */

#define increment_short_value(CPU, REG)    (REG) = inc((CPU), (REG))

#define increment_long_value(HIGH, LOW)    int __result =                 \
                                             concat_bytes((HIGH), (LOW)); \
                                           ++__result;                    \
                                           (HIGH) = high_byte(__result);  \
                                           (LOW) = low_byte(__result);    \

#define increment_indirect(CPU, HIGH, LOW) gb_address_t __address =       \
                                             concat_bytes((HIGH), (LOW)); \
                                           indirect_inc((CPU), __address);\

#define decrement_short_value(CPU, REG)    (REG) = dec((CPU), (REG))

#define decrement_long_value(HIGH, LOW)    int __result =                 \
                                             concat_bytes((HIGH), (LOW)); \
                                           --__result;                    \
                                           (HIGH) = high_byte(__result);  \
                                           (LOW) = low_byte(__result);    \

#define decrement_indirect(CPU, HIGH, LOW) gb_address_t __address =       \
                                             concat_bytes((HIGH), (LOW)); \
                                           indirect_dec((CPU), __address);\

#define ADD16(CPU, REG1, REG2, REG3, REG4)  {\
  uint16_t result = add16((CPU), (REG1), (REG2), (REG3), (REG4));\
  (REG1) = high_byte(result);\
  (REG2) = low_byte(result);}

#define ADD8(CPU, REG) {\
  (CPU)->A = add8((CPU), (REG));\
}

#define ADC8(CPU, REG) {\
  (CPU)->A = adc8((CPU), (REG));\
}

#define SUB8(CPU, REG) {\
  (CPU)->A = sub8((CPU), (REG));\
}

#define SBC8(CPU, REG) {\
  (CPU)->A = sbc8((CPU), (REG));\
}

#define AND8(CPU, REG) {\
  (CPU)->A = and8((CPU), (REG));\
}

#define OR8(CPU, REG) {\
  (CPU)->A = or8((CPU), (REG));\
}

#define XOR8(CPU, REG) {\
  (CPU)->A = xor8((CPU), (REG));\
}

#define CP8(CPU, REG) {\
  sub8((CPU), (REG));\
}

/* +++++++++++++++++++++++++++++++++++++++++++++++
 * +                  JUMPS                      +
 * +++++++++++++++++++++++++++++++++++++++++++++++
 */
void jump(cpu_t *cpu, gb_address_t address);

#define JP_IF_R16(CPU, COND) {\
  uint8_t low = cpu_fetch((CPU));\
  uint8_t high = cpu_fetch((CPU));\
  gb_address_t address = concat_bytes(high, low);\
  if (!(COND)) return 12;\
  jump((CPU), address);}

#define JP_R16(CPU) JP_IF_R16((CPU), 1)

#define JR_IF_R8(CPU, COND) {\
  int8_t offset = (int8_t) cpu_fetch((CPU));\
  if (!(COND)) return 8;\
  (CPU)->pc += offset;}

#define JR_R8(CPU)  JR_IF_R8((CPU), 1)

/* +++++++++++++++++++++++++++++++++++++++++++++++
 * +                  STACK                      +
 * +++++++++++++++++++++++++++++++++++++++++++++++
 */
void push(cpu_t *cpu, uint8_t high, uint8_t low);

uint16_t pop(cpu_t *cpu);

#define PUSH(CPU, HIGH, LOW) push((CPU), (HIGH), (LOW))

#define POP(CPU, HIGH, LOW) {\
  uint16_t value = pop((CPU));\
  (HIGH) = high_byte(value);\
  (LOW) = low_byte(value);}

#define CALL_IF(CPU, COND) {\
  uint8_t low = cpu_fetch((CPU));\
  uint8_t high = cpu_fetch((CPU));\
  if (!(COND)) return 12;\
  ++cpu->pc;\
  PUSH((CPU), high_byte((CPU)->pc), low_byte((CPU)->pc));\
  jump((CPU), concat_bytes(high, low));}

#define CALL(CPU) CALL_IF((CPU), 1)

#define RET_IF(CPU, COND) {\
  uint8_t high = 0, low = 0;\
  if (!(COND)) return 8;\
  POP((CPU), high, low);\
  jump((CPU), concat_bytes(high, low));}

#define RET(CPU) RET_IF((CPU), 1)

#define RST(CPU, VAL) {\
  ++(CPU)->pc;\
  PUSH((CPU), high_byte((CPU)->pc), low_byte((CPU)->pc));\
  jump((CPU), (VAL));}

