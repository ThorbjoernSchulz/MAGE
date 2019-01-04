#include "driver/testing.h"

TEST(test_cb_res, {
  gb_address_t ip = 0;

  __test_write(cpu, ip++, 0x3E); /* LD A, 0x80 */
  __test_write(cpu, ip++, 0x80);

  __test_write(cpu, ip++, 0xCB);
  __test_write(cpu, ip++, 0xBF);

  __test_write(cpu, ip++, 0x00);

  run(cpu);

  assert(!cpu->A);
})

TEST(test_cb_set_hl, {
  gb_address_t ip = 0;

  __test_write(cpu, ip++, 0x2E); /* LD L, 0x20 */
  __test_write(cpu, ip++, 0x20);

  __test_write(cpu, ip++, 0xCB); /* SET 0, (HL) */
  __test_write(cpu, ip++, 0xC6);

  __test_write(cpu, ip++, 0x46); /* LD B, (HL) */

  __test_write(cpu, ip++, 0x00);

  run(cpu);

  assert(cpu->B == 0x01);
})
