#include "cpu.h"
#include "instructions.h"
#include "interrupts.h"

#define is_halt(I)  I == 0x76

uint8_t cpu_do_execute(cpu_t *cpu, uint8_t instruction);

void timer_update(cpu_t *cpu, uint8_t cycles);

void div_update(cpu_t *cpu, uint8_t cycles);

extern void die(const char *s);

static const uint8_t instruction_duration[] = {
    4, 12, 8, 8, 4, 4, 8, 4, 20, 8, 8, 8, 4, 4, 8, 4,
    4, 12, 8, 8, 4, 4, 8, 4, 12, 8, 8, 8, 4, 4, 8, 4,
    12, 12, 8, 8, 4, 4, 8, 4, 12, 8, 8, 8, 4, 4, 8, 4,
    12, 12, 8, 8, 12, 12, 12, 4, 12, 8, 8, 8, 4, 4, 8,
    4, 4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8,
    4, 4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8,
    4, 4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8,
    4, 8, 8, 8, 8, 8, 8, 4, 8, 4, 4, 4, 4, 4, 4, 8,
    4, 4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8,
    4, 4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8,
    4, 4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8,
    4, 4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8,
    4, 20, 12, 16, 16, 24, 16, 8, 16, 20, 16, 16, 0, 24, 24,
    8, 16, 20, 0, 12, 16, 24, 16, 8, 16, 20, 16, 16, 24, 8, 16,
    12, 12, 8, 0, 0, 16, 8, 16, 16, 4, 16, 0, 0, 0, 8, 16,
    12, 12, 8, 4, 0, 16, 8, 16, 12, 8, 16, 4, 0, 0, 8, 16
};

uint8_t cpu_update_state(cpu_t *cpu, debugger_t *debugger) {
  uint8_t cycles = 0;

  uint8_t instruction = cpu_read(cpu, cpu->pc);

  /* ei delay */
  if (cpu->ei_instruction_used) {
    cpu->ei_instruction_used = false;
    cpu->interrupts_enabled = true;
  }

  cycles += cpu_do_execute(cpu, instruction);

  if (!cpu->halted) ++cpu->pc;

  cycles += process_interrupts(cpu);

  div_update(cpu, cycles);
  timer_update(cpu, cycles);

  return cycles;
}

unsigned int sleep(unsigned int);

void freeze(void) { while (1) { /* nothing to do */ sleep(5); }}

uint8_t cpu_do_execute(cpu_t *cpu, uint8_t instruction) {
  switch (instruction) {
    case 0x00: /* NOP */
      break;

    case 0x01: /* LD BC, d16 */ {
      load_long_value(cpu, cpu->B, cpu->C);
      break;
    }

    case 0x02: /* LD (BC), A */ {
      store_short_value(cpu, cpu->B, cpu->C, cpu->A);
      break;
    }

    case 0x03: /* INC BC */ {
      INC2(cpu->B, cpu->C);
      break;
    }

    case 0x04: /* INC B */
      INC(cpu, cpu->B);
      break;

    case 0x05: /* DEC B*/
      DEC(cpu, cpu->B);
      break;

    case 0x06: /* LD B, d8 */
      cpu->B = cpu_fetch(cpu);
      break;

    case 0x07: /* RLCA */ {
      rlca(cpu);
      break;
    }

    case 0x08: /* LD (a16), SP */ {
      store_long_value(cpu, cpu->S, cpu->P);
      break;
    }

    case 0x09: /* ADD HL, BC */ {
      ADD16(cpu, cpu->H, cpu->L, cpu->B, cpu->C);
      break;
    }

    case 0x0A: /* LD A, (BC) */ {
      load_short_value(cpu, cpu->A, cpu->B, cpu->C);
      break;
    }

    case 0x0B: /* DEC BC */ {
      DEC2(cpu->B, cpu->C);
      break;
    }

    case 0x0C: /* INC C */
      INC(cpu, cpu->C);
      break;

    case 0x0D: /* DEC C */
      DEC(cpu, cpu->C);
      break;

    case 0x0E: /* LD C, d8 */
      cpu->C = cpu_fetch(cpu);
      break;

    case 0x0F: /* RRCA */ {
      rrca(cpu);
      break;
    }

    case 0x10: /* STOP */
      if (cpu_fetch(cpu)) --cpu->pc;
      /* TODO: Implement this */
      break;

    case 0x11: /* LD DE, d16 */ {
      load_long_value(cpu, cpu->D, cpu->E);
      break;
    }

    case 0x12: /* LD (DE), A */ {
      store_short_value(cpu, cpu->D, cpu->E, cpu->A);
      break;
    }

    case 0x13: /* INC DE */ {
      INC2(cpu->D, cpu->E);
      break;
    }

    case 0x14: /* INC D */
      INC(cpu, cpu->D);
      break;

    case 0x15: /* DEC D*/
      DEC(cpu, cpu->D);
      break;

    case 0x16: /* LD D, d8 */
      cpu->D = cpu_fetch(cpu);
      break;

    case 0x17: /* RLA */ {
      rla(cpu);
      break;
    }

    case 0x18: /* JR r8 */ {
      JR_R8(cpu);
      break;
    }

    case 0x19: /* ADD HL, DE */ {
      ADD16(cpu, cpu->H, cpu->L, cpu->D, cpu->E);
      break;
    }

    case 0x1A: /* LD A, (DE) */ {
      load_short_value(cpu, cpu->A, cpu->D, cpu->E);
      break;
    }

    case 0x1B: /* DEC DE */ {
      DEC2(cpu->D, cpu->E);
      break;
    }

    case 0x1C: /* INC E */
      INC(cpu, cpu->E);
      break;

    case 0x1D: /* DEC E */
      DEC(cpu, cpu->E);
      break;

    case 0x1E: /* LD E, d8 */
      cpu->E = cpu_fetch(cpu);
      break;

    case 0x1F: /* RRA */ {
      rra(cpu);
      break;
    }

    case 0x20: /* JR NZ, r8 */ {
      JR_IF_R8(cpu, !(zero_set(cpu)));
      break;
    }

    case 0x21: /* LD HL, d16 */ {
      load_long_value(cpu, cpu->H, cpu->L);
      break;
    }

    case 0x22: /* LD (HL+), A */ {
      store_short_value(cpu, cpu->H, cpu->L, cpu->A);
      INC2(cpu->H, cpu->L);
      break;
    }

    case 0x23: /* INC HL */ {
      INC2(cpu->H, cpu->L);
      break;
    }

    case 0x24: /* INC H */
      INC(cpu, cpu->H);
      break;

    case 0x25: /* DEC H */
      DEC(cpu, cpu->H);
      break;

    case 0x26: /* LD H, d8 */
      cpu->H = cpu_fetch(cpu);
      break;

    case 0x27: /* DAA */ {
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

      break;
    }

    case 0x28: /* JR Z, r8 */
    JR_IF_R8(cpu, zero_set(cpu));
      break;

    case 0x29: /* ADD HL, HL */
    ADD16(cpu, cpu->H, cpu->L, cpu->H, cpu->L);
      break;

    case 0x2A: /* LD A, (HL+) */ {
      load_short_value(cpu, cpu->A, cpu->H, cpu->L);
      INC2(cpu->H, cpu->L);
      break;
    }

    case 0x2B: /* DEC HL */ {
      DEC2(cpu->H, cpu->L);
      break;
    }

    case 0x2C: /* INC L */
      INC(cpu, cpu->L);
      break;

    case 0x2D: /* DEC L */
      DEC(cpu, cpu->L);
      break;

    case 0x2E: /* LD L, d8 */
      cpu->L = cpu_fetch(cpu);
      break;

    case 0x2F: /* CPL */ {
      cpu->A = ~(cpu->A);
      set_sub_flag(cpu);
      set_hcarry_flag(cpu);
      break;
    }

    case 0x30: /* JR NC, r8 */ {
      JR_IF_R8(cpu, !carry_set(cpu));
      break;
    }

    case 0x31: /* LD SP, d16 */ {
      load_long_value(cpu, cpu->S, cpu->P);
      break;
    }

    case 0x32: /* LD (HL-), A */ {
      store_short_value(cpu, cpu->H, cpu->L, cpu->A);
      DEC2(cpu->H, cpu->L);
      break;
    }

    case 0x33: /* INC SP */ {
      INC2(cpu->S, cpu->P);
      break;
    }

    case 0x34: /* INC (HL) */ {
      bool carry_was_set = carry_set(cpu);
      clear_flags(cpu);

      gb_address_t address = concat_bytes(cpu->H, cpu->L);
      uint8_t byte = cpu_read(cpu, address);

      if ((byte & 0xF) == 0xF) set_hcarry_flag(cpu);

      ++byte;

      if (!byte) set_zero_flag(cpu);

      cpu_write(cpu, address, byte);
      if (carry_was_set) set_carry_flag(cpu);
      break;
    }

    case 0x35: /* DEC (HL) */ {
      bool carry_was_set = carry_set(cpu);
      clear_flags(cpu);
      set_sub_flag(cpu);

      gb_address_t address = concat_bytes(cpu->H, cpu->L);
      uint8_t byte = cpu_read(cpu, address);

      if ((byte & 0xF) == 0) set_hcarry_flag(cpu);

      --byte;

      if (!byte) set_zero_flag(cpu);

      cpu_write(cpu, address, byte);
      if (carry_was_set) set_carry_flag(cpu);
      break;
    }

    case 0x36: /* LD (HL), d8 */ {
      gb_address_t address = concat_bytes(cpu->H, cpu->L);
      uint8_t byte = cpu_fetch(cpu);
      cpu_write(cpu, address, byte);
      break;
    }

    case 0x37: /* SCF */ {
      set_carry_flag(cpu);
      set_add_flag(cpu);
      unset_hcarry_flag(cpu);
      break;
    }

    case 0x38: /* JR C, r8 */ {
      JR_IF_R8(cpu, carry_set(cpu));
      break;
    }

    case 0x39: /* ADD HL, SP */ {
      ADD16(cpu, cpu->H, cpu->L, cpu->S, cpu->P);
      break;
    }

    case 0x3A: /* LD A, (HL-) */ {
      load_short_value(cpu, cpu->A, cpu->H, cpu->L);
      DEC2(cpu->H, cpu->L);
      break;
    }

    case 0x3B: /* DEC SP */ {
      DEC2(cpu->S, cpu->P);
      break;
    }

    case 0x3C: /* INC A */
      INC(cpu, cpu->A);
      break;

    case 0x3D: /* DEC A */
      DEC(cpu, cpu->A);
      break;

    case 0x3E: /* LD A, d8 */
      cpu->A = cpu_fetch(cpu);
      break;

    case 0x3F: /* CCF */ {
      if (carry_set(cpu))
        unset_carry_flag(cpu);
      else
        set_carry_flag(cpu);

      set_add_flag(cpu);
      unset_hcarry_flag(cpu);
      break;
    }

    case 0x40: /* LD B, B */
      cpu->B = cpu->B;
      break;

    case 0x41: /* LD B, C */
      cpu->B = cpu->C;
      break;

    case 0x42: /* LD B, D */
      cpu->B = cpu->D;
      break;

    case 0x43: /* LD B, E */
      cpu->B = cpu->E;
      break;

    case 0x44: /* LD B, H */
      cpu->B = cpu->H;
      break;

    case 0x45: /* LD B, L */
      cpu->B = cpu->L;
      break;

    case 0x46: /* LD B, (HL) */ {
      load_short_value(cpu, cpu->B, cpu->H, cpu->L);
      break;
    }

    case 0x47: /* LD B, A */
      cpu->B = cpu->A;
      break;

    case 0x48: /* LD C, B */
      cpu->C = cpu->B;
      break;

    case 0x49: /* LD C, C */
      cpu->C = cpu->C;
      break;

    case 0x4A: /* LD C, D */
      cpu->C = cpu->D;
      break;

    case 0x4B: /* LD C, E */
      cpu->C = cpu->E;
      break;

    case 0x4C: /* LD C, H */
      cpu->C = cpu->H;
      break;

    case 0x4D: /* LD C, L*/
      cpu->C = cpu->L;
      break;

    case 0x4E: /* LD C, (HL) */ {
      load_short_value(cpu, cpu->C, cpu->H, cpu->L);
      break;
    }

    case 0x4F: /* LD C, A */
      cpu->C = cpu->A;
      break;

    case 0x50: /* LD D, B */
      cpu->D = cpu->B;
      break;

    case 0x51: /* LD D, C */
      cpu->D = cpu->C;
      break;

    case 0x52: /* LD D, D */
      cpu->D = cpu->D;
      break;

    case 0x53: /* LD D, E */
      cpu->D = cpu->E;
      break;

    case 0x54: /* LD D, H */
      cpu->D = cpu->H;
      break;

    case 0x55: /* LD D, L */
      cpu->D = cpu->L;
      break;

    case 0x56: /* LD D, (HL) */ {
      load_short_value(cpu, cpu->D, cpu->H, cpu->L);
      break;
    }

    case 0x57: /* LD D, A */
      cpu->D = cpu->A;
      break;

    case 0x58: /* LD E, B */
      cpu->E = cpu->B;
      break;

    case 0x59: /* LD E, C */
      cpu->E = cpu->C;
      break;

    case 0x5A: /* LD E, D */
      cpu->E = cpu->D;
      break;

    case 0x5B: /* LD E, E */
      cpu->E = cpu->E;
      break;

    case 0x5C: /* LD E, H */
      cpu->E = cpu->H;
      break;

    case 0x5D: /* LD E, L*/
      cpu->E = cpu->L;
      break;

    case 0x5E: /* LD E, (HL) */ {
      load_short_value(cpu, cpu->E, cpu->H, cpu->L);
      break;
    }

    case 0x5F: /* LD E, A */
      cpu->E = cpu->A;
      break;

    case 0x60: /* LD H, B */
      cpu->H = cpu->B;
      break;

    case 0x61: /* LD H, C */
      cpu->H = cpu->C;
      break;

    case 0x62: /* LD H, D */
      cpu->H = cpu->D;
      break;

    case 0x63: /* LD H, E */
      cpu->H = cpu->E;
      break;

    case 0x64: /* LD H, H */
      cpu->H = cpu->H;
      break;

    case 0x65: /* LD H, L */
      cpu->H = cpu->L;
      break;

    case 0x66: /* LD H, (HL) */ {
      load_short_value(cpu, cpu->H, cpu->H, cpu->L);
      break;
    }

    case 0x67: /* LD H, A */
      cpu->H = cpu->A;
      break;

    case 0x68: /* LD L, B */
      cpu->L = cpu->B;
      break;

    case 0x69: /* LD L, C */
      cpu->L = cpu->C;
      break;

    case 0x6A: /* LD L, D */
      cpu->L = cpu->D;
      break;

    case 0x6B: /* LD L, E */
      cpu->L = cpu->E;
      break;

    case 0x6C: /* LD L, H */
      cpu->L = cpu->H;
      break;

    case 0x6D: /* LD L, L*/
      cpu->L = cpu->L;
      break;

    case 0x6E: /* LD L, (HL) */ {
      load_short_value(cpu, cpu->L, cpu->H, cpu->L);
      break;
    }

    case 0x6F: /* LD L, A */
      cpu->L = cpu->A;
      break;

    case 0x70: /* LD (HL), B */ {
      store_short_value(cpu, cpu->H, cpu->L, cpu->B);
      break;
    }

    case 0x71: /* LD (HL), C */ {
      store_short_value(cpu, cpu->H, cpu->L, cpu->C);
      break;
    }

    case 0x72: /* LD (HL), D */ {
      store_short_value(cpu, cpu->H, cpu->L, cpu->D);
      break;
    }

    case 0x73: /* LD (HL), E */ {
      store_short_value(cpu, cpu->H, cpu->L, cpu->E);
      break;
    }

    case 0x74: /* LD (HL), H */ {
      store_short_value(cpu, cpu->H, cpu->L, cpu->H);
      break;
    }

    case 0x75: /* LD (HL), L */ {
      store_short_value(cpu, cpu->H, cpu->L, cpu->L);
      break;
    }

    case 0x76: /* HALT */ {
      /* HALT bug */
      if (!cpu->interrupts_enabled && interrupts_ready(cpu)) {
        uint8_t next_instruction = cpu_read(cpu, cpu->pc + (uint16_t) 1);
        if (is_halt(next_instruction)) freeze();

        return 4 + cpu_do_execute(cpu, next_instruction);
      }

      cpu->halted = true;
      break;
    }

    case 0x77: /* LD (HL), A */ {
      store_short_value(cpu, cpu->H, cpu->L, cpu->A);
      break;
    }


    case 0x78: /* LD A, B */
      cpu->A = cpu->B;
      break;

    case 0x79: /* LD A, C */
      cpu->A = cpu->C;
      break;

    case 0x7A: /* LD A, D */
      cpu->A = cpu->D;
      break;

    case 0x7B: /* LD A, E */
      cpu->A = cpu->E;
      break;

    case 0x7C: /* LD A, H */
      cpu->A = cpu->H;
      break;

    case 0x7D: /* LD A, L*/
      cpu->A = cpu->L;
      break;

    case 0x7E: /* LD A, (HL) */ {
      load_short_value(cpu, cpu->A, cpu->H, cpu->L);
      break;
    }

    case 0x7F: /* LD A, A */
      cpu->A = cpu->A;
      break;

    case 0x80: /* ADD A, B */
    ADD8(cpu, cpu->B);
      break;

    case 0x81: /* ADD A, C */
    ADD8(cpu, cpu->C);
      break;

    case 0x82: /* ADD A, D */
    ADD8(cpu, cpu->D);
      break;

    case 0x83: /* ADD A, E */
    ADD8(cpu, cpu->E);
      break;

    case 0x84: /* ADD A, H */
    ADD8(cpu, cpu->H);
      break;

    case 0x85: /* ADD A, L */
    ADD8(cpu, cpu->L);
      break;

    case 0x86: /* ADD A, (HL) */ {
      uint8_t hl;
      load_short_value(cpu, hl, cpu->H, cpu->L);
      ADD8(cpu, hl);
      break;
    }

    case 0x87: /* ADD A, A */
    ADD8(cpu, cpu->A);
      break;

    case 0x88: /* ADC A, B */
    ADC8(cpu, cpu->B);
      break;

    case 0x89: /* ADC A, C */
    ADC8(cpu, cpu->C);
      break;

    case 0x8A: /* ADC A, D */
    ADC8(cpu, cpu->D);
      break;

    case 0x8B: /* ADC A, E */
    ADC8(cpu, cpu->E);
      break;

    case 0x8C: /* ADC A, H */
    ADC8(cpu, cpu->H);
      break;

    case 0x8D: /* ADC A, L */
    ADC8(cpu, cpu->L);
      break;

    case 0x8E: /* ADC A, (HL) */ {
      uint8_t hl;
      load_short_value(cpu, hl, cpu->H, cpu->L);
      ADC8(cpu, hl);
      break;
    }

    case 0x8F: /* ADC A, A */
    ADC8(cpu, cpu->A);
      break;

    case 0x90: /* SUB A, B */
    SUB8(cpu, cpu->B);
      break;

    case 0x91: /* SUB A, C */
    SUB8(cpu, cpu->C);
      break;

    case 0x92: /* SUB A, D */
    SUB8(cpu, cpu->D);
      break;

    case 0x93: /* SUB A, E */
    SUB8(cpu, cpu->E);
      break;

    case 0x94: /* SUB A, H */
    SUB8(cpu, cpu->H);
      break;

    case 0x95: /* SUB A, L */
    SUB8(cpu, cpu->L);
      break;

    case 0x96: /* SUB A, (HL) */ {
      uint8_t hl;
      load_short_value(cpu, hl, cpu->H, cpu->L);
      SUB8(cpu, hl);
      break;
    }

    case 0x97: /* SUB A, A */
    SUB8(cpu, cpu->A);
      break;

    case 0x98: /* SBC A, B */
    SBC8(cpu, cpu->B);
      break;

    case 0x99: /* SBC A, C */
    SBC8(cpu, cpu->C);
      break;

    case 0x9A: /* SBC A, D */
    SBC8(cpu, cpu->D);
      break;

    case 0x9B: /* SBC A, E */
    SBC8(cpu, cpu->E);
      break;

    case 0x9C: /* SBC A, H */
    SBC8(cpu, cpu->H);
      break;

    case 0x9D: /* SBC A, L */
    SBC8(cpu, cpu->L);
      break;

    case 0x9E: /* SBC A, (HL) */ {
      uint8_t hl;
      load_short_value(cpu, hl, cpu->H, cpu->L);
      SBC8(cpu, hl);
      break;
    }

    case 0x9F: /* SBC A, A */
    SBC8(cpu, cpu->A);
      break;

    case 0xA0: /* AND A, B */
    AND8(cpu, cpu->B);
      break;

    case 0xA1: /* AND A, C */
    AND8(cpu, cpu->C);
      break;

    case 0xA2: /* AND A, D */
    AND8(cpu, cpu->D);
      break;

    case 0xA3: /* AND A, E */
    AND8(cpu, cpu->E);
      break;

    case 0xA4: /* AND A, H */
    AND8(cpu, cpu->H);
      break;

    case 0xA5: /* AND A, L */
    AND8(cpu, cpu->L);
      break;

    case 0xA6: /* AND A, (HL) */ {
      uint8_t hl;
      load_short_value(cpu, hl, cpu->H, cpu->L);
      AND8(cpu, hl);
      break;
    }

    case 0xA7: /* AND A, A */
    AND8(cpu, cpu->A);
      break;

    case 0xA8: /* XOR A, B */
    XOR8(cpu, cpu->B);
      break;

    case 0xA9: /* XOR A, C */
    XOR8(cpu, cpu->C);
      break;

    case 0xAA: /* XOR A, D */
    XOR8(cpu, cpu->D);
      break;

    case 0xAB: /* XOR A, E */
    XOR8(cpu, cpu->E);
      break;

    case 0xAC: /* XOR A, H */
    XOR8(cpu, cpu->H);
      break;

    case 0xAD: /* XOR A, L */
    XOR8(cpu, cpu->L);
      break;

    case 0xAE: /* XOR A, (HL) */ {
      uint8_t hl;
      load_short_value(cpu, hl, cpu->H, cpu->L);
      XOR8(cpu, hl);
      break;
    }

    case 0xAF: /* XOR A, A */
    XOR8(cpu, cpu->A);
      break;

    case 0xB0: /* OR A, B */
    OR8(cpu, cpu->B);
      break;

    case 0xB1: /* OR A, C */
    OR8(cpu, cpu->C);
      break;

    case 0xB2: /* OR A, D */
    OR8(cpu, cpu->D);
      break;

    case 0xB3: /* OR A, E */
    OR8(cpu, cpu->E);
      break;

    case 0xB4: /* OR A, H */
    OR8(cpu, cpu->H);
      break;

    case 0xB5: /* OR A, L */
    OR8(cpu, cpu->L);
      break;

    case 0xB6: /* OR A, (HL) */ {
      uint8_t hl;
      load_short_value(cpu, hl, cpu->H, cpu->L);
      OR8(cpu, hl);
      break;
    }

    case 0xB7: /* OR A, A */
    OR8(cpu, cpu->A);
      break;

    case 0xB8: /* CP A, B */
    CP8(cpu, cpu->B);
      break;

    case 0xB9: /* CP A, C */
    CP8(cpu, cpu->C);
      break;

    case 0xBA: /* CP A, D */
    CP8(cpu, cpu->D);
      break;

    case 0xBB: /* CP A, E */
    CP8(cpu, cpu->E);
      break;

    case 0xBC: /* CP A, H */
    CP8(cpu, cpu->H);
      break;

    case 0xBD: /* CP A, L */
    CP8(cpu, cpu->L);
      break;

    case 0xBE: /* CP A, (HL) */ {
      uint8_t hl;
      load_short_value(cpu, hl, cpu->H, cpu->L);
      CP8(cpu, hl);
      break;
    }

    case 0xBF: /* CP A, A */
    CP8(cpu, cpu->A);
      break;

    case 0xC0: /* RET NZ */
    RET_IF(cpu, !zero_set(cpu));
      break;

    case 0xC1: /* POP BC */
    POP(cpu, cpu->B, cpu->C);
      break;

    case 0xC2: /* JP NZ, a16 */
    JP_IF_R16(cpu, !zero_set(cpu));
      break;

    case 0xC3: /* JP a16 */
    JP_R16(cpu);
      break;

    case 0xC4: /* CALL NZ, a16 */
    CALL_IF(cpu, !zero_set(cpu));
      break;

    case 0xC5: /* PUSH BC */
      PUSH(cpu, cpu->B, cpu->C);
      break;

    case 0xC6: /* ADD A, d8 */
    ADD8(cpu, cpu_fetch(cpu));
      break;

    case 0xC7: /* RST 00H */
    RST(cpu, 0x00);
      break;

    case 0xC8: /* RET Z */
    RET_IF(cpu, zero_set(cpu));
      break;

    case 0xC9: /* RET */ {
      RET(cpu);
      break;
    }

    case 0xCA: /* JP Z, a16 */
    JP_IF_R16(cpu, zero_set(cpu));
      break;

    case 0xCB: /* PREFIX CB */
      return cb_inst_execute(cpu, cpu_fetch(cpu));

    case 0xCC: /* CALL Z, a16 */
    CALL_IF(cpu, zero_set(cpu));
      break;

    case 0xCD: /* CALL a16 */
    CALL(cpu);
      break;

    case 0xCE: /* ADC A, d8 */
    ADC8(cpu, cpu_fetch(cpu));
      break;

    case 0xCF: /* RST 08H */
    RST(cpu, 0x08);
      break;

    case 0xD0: /* RET NC */
    RET_IF(cpu, !carry_set(cpu));
      break;

    case 0xD1: /* POP DE */
    POP(cpu, cpu->D, cpu->E);
      break;

    case 0xD2: /* JP NC, a16 */
    JP_IF_R16(cpu, !carry_set(cpu));
      break;

    case 0xD3: /* --- */
      die("Instruction not implemented");

    case 0xD4: /* CALL NC, a16 */
    CALL_IF(cpu, !carry_set(cpu));
      break;

    case 0xD5: /* PUSH DE */
      PUSH(cpu, cpu->D, cpu->E);
      break;

    case 0xD6: /* SUB A, d8 */
    SUB8(cpu, cpu_fetch(cpu));
      break;

    case 0xD7: /* RST 10H */
    RST(cpu, 0x10);
      break;

    case 0xD8: /* RET C */
    RET_IF(cpu, carry_set(cpu));
      break;

    case 0xD9: /* RETI */
    RET(cpu);
      cpu->interrupts_enabled = true;
      break;

    case 0xDA: /* JP C, a16 */
    JP_IF_R16(cpu, carry_set(cpu));
      break;

    case 0xDB: /* --- */
      die("Instruction not implemented");

    case 0xDC: /* CALL C, a16 */
    CALL_IF(cpu, carry_set(cpu));
      break;

    case 0xDD: /* --- */
      die("Instruction not implemented");

    case 0xDE: /* SBC A, d8 */
    SBC8(cpu, cpu_fetch(cpu));
      break;

    case 0xDF: /* RST 18H */
    RST(cpu, 0x18);
      break;

    case 0xE0: /* LDH (a8), A */ {
      gb_address_t address = (gb_address_t) (0xFF00 + cpu_fetch(cpu));
      cpu_write(cpu, address, cpu->A);
      break;
    }

    case 0xE1: /* POP HL */
    POP(cpu, cpu->H, cpu->L);
      break;

    case 0xE2: /* LD (C), A */ {
      gb_address_t address = (gb_address_t) (0xFF00 + cpu->C);
      cpu_write(cpu, address, cpu->A);
      break;
    }

    case 0xE3:
    case 0xE4: /* --- */
      die("Instruction not implemented");

    case 0xE5: /* PUSH HL */
      PUSH(cpu, cpu->H, cpu->L);
      break;

    case 0xE6: /* AND d8 */
    AND8(cpu, cpu_fetch(cpu));
      break;

    case 0xE7: /* RST 20H */
    RST(cpu, 0x20);
      break;

    case 0xE8: /* ADD SP, r8 */ {
      uint16_t result = add16s(cpu, cpu->S, cpu->P, cpu_fetch(cpu));
      cpu->S = high_byte(result);
      cpu->P = low_byte(result);
      break;
    }

    case 0xE9: /* JP (HL) */ {
      jump(cpu, concat_bytes(cpu->H, cpu->L));
      break;
    }

    case 0xEA: /* LD (a16), A */ {
      uint8_t low = cpu_fetch(cpu);
      uint8_t high = cpu_fetch(cpu);
      cpu_write(cpu, concat_bytes(high, low), cpu->A);
      break;
    }

    case 0xEB:
    case 0xEC:
    case 0xED: /* --- */
      die("Instruction not implemented");

    case 0xEE: /* XOR d8 */
    XOR8(cpu, cpu_fetch(cpu));
      break;

    case 0xEF: /* RST 28H */
    RST(cpu, 0x28);
      break;

    case 0xF0: /* LDH A, (a8) */ {
      gb_address_t address = (gb_address_t) (0xFF00 + cpu_fetch(cpu));
      cpu->A = cpu_read(cpu, address);
      break;
    }

    case 0xF1: /* POP AF */
    POP(cpu, cpu->A, cpu->F);
      cpu->F &= 0xF0;
      break;

    case 0xF2: /* LD A, (C)*/ {
      gb_address_t address = (gb_address_t) (0xFF00 + cpu->C);
      cpu->A = cpu_read(cpu, address);
      break;
    }

    case 0xF3: /* DI */
      cpu->interrupts_enabled = false;
      break;

    case 0xF4: /* --- */
      die("Instruction not implemented");

    case 0xF5: /* PUSH AF */
      PUSH(cpu, cpu->A, cpu->F);
      break;

    case 0xF6: /* OR d8 */
    OR8(cpu, cpu_fetch(cpu));
      break;

    case 0xF7: /* RST 30H */
    RST(cpu, 0x30);
      break;

    case 0xF8: /* LD HL, SP+r8 */ {
      uint8_t high = cpu->S;
      uint8_t low = cpu->P;
      uint16_t result = add16s(cpu, high, low, cpu_fetch(cpu));
      cpu->H = high_byte(result);
      cpu->L = low_byte(result);
      break;
    }

    case 0xF9: /* LD SP, HL */ {
      cpu->S = cpu->H;
      cpu->P = cpu->L;
      break;
    }

    case 0xFA: /* LD A, (a16) */ {
      uint8_t low = cpu_fetch(cpu);
      uint8_t high = cpu_fetch(cpu);
      gb_address_t address = concat_bytes(high, low);
      cpu->A = cpu_read(cpu, address);
      break;
    }

    case 0xFB: /* EI */
      cpu->ei_instruction_used = true;
      break;

    case 0xFC:
    case 0xFD: /* --- */
      die("Instruction not implemented");

    case 0xFE: /* CP d8 */
    CP8(cpu, cpu_fetch(cpu));
      break;

    case 0xFF: /* RST 38H */
    RST(cpu, 0x38);
      break;

    default:
      break;
  }

  return instruction_duration[instruction];
}
