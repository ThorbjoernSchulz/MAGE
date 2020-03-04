#include "driver/testing.h"
#include "src/cpu/instructions.h"

TEST(test_push_pop,
  gb_address_t ip = 0;
  /* setup stack */
  __test_write(cpu, ip++, 0x31); /* LD SP, d16 */
  __test_write(cpu, ip++, 0xF0);
  __test_write(cpu, ip++, 0xFF);

  __test_write(cpu, ip++, 0x01); /* LD BC, d16 */
  __test_write(cpu, ip++, 0xBB);
  __test_write(cpu, ip++, 0xAA);

  __test_write(cpu, ip++, 0x11); /* LD DE, d16 */
  __test_write(cpu, ip++, 0xDD);
  __test_write(cpu, ip++, 0xCC);

  __test_write(cpu, ip++, 0xC5); /* PUSH BC */
  __test_write(cpu, ip++, 0xD5); /* PUSH DE */

  __test_write(cpu, ip++, 0xC1); /* POP BC */
  __test_write(cpu, ip++, 0xD1); /* POP DE */

  __test_write(cpu, ip++, 0x00);

  run(cpu);

  assert(cpu->B == 0xCC && cpu->C == 0xDD);
  assert(cpu->D == 0xAA && cpu->E == 0xBB);
)

TEST(test_call_if,
  gb_address_t ip = 0;
  /* setup stack */
  __test_write(cpu, ip++, 0x31); /* LD SP, d16 */
  __test_write(cpu, ip++, 0xF0);
  __test_write(cpu, ip++, 0xFF);

  __test_write(cpu, ip++, 0xFB); /* EI */
  __test_write(cpu, ip++, 0x3E); /* LD A, 0xFF */
  __test_write(cpu, ip++, 0xFF);
  __test_write(cpu, ip++, 0xFE); /* CP 0xA0 */
  __test_write(cpu, ip++, 0xA0);
  __test_write(cpu, ip++, 0xD4); /* CALL NC, a16 */
  __test_write(cpu, ip++, 0x00);
  __test_write(cpu, ip++, 0xF0);

  __test_write(cpu, ip++, 0x00);

  run(cpu);

  assert(cpu->pc == 0xF000);
)

TEST(test_rst_00h,
  gb_address_t ip = 0;
  __test_write(cpu, ip++, 0xCA); /* JP Z, d 16 */
  __test_write(cpu, ip++, 0xCD);
  __test_write(cpu, ip++, 0xAB);

  /* setup stack */
  __test_write(cpu, ip++, 0x31); /* LD SP, d16 */
  __test_write(cpu, ip++, 0xF0);
  __test_write(cpu, ip++, 0xFF);

  __test_write(cpu, ip++, 0xA8); /* XOR A, setting the Z flag */
  __test_write(cpu, ip++, 0xC7); /* RST 0x00 */

  __test_write(cpu, ip++, 0x00);

  run(cpu);

  assert(cpu->pc == 0xABCD);
  assert(pop(cpu) == 0x08);
)
