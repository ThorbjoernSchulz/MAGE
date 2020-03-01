#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "testing.h"
#include "src/mmu.h"

#include "src/memory_handler.h"

void die(const char *s) {
  fputs(s, stderr);
  abort();
}

void run(cpu_t *cpu) {
  while (cpu_read(cpu, cpu->pc) != 0x00) update_cpu_state(cpu, NULL);
}

static uint8_t memory[0xC000];
static mem_handler_t test_mem_handler;

static void test_write(mem_handler_t *this, gb_address_t offset, uint8_t value) {
  memory[offset] = value;
}
static uint8_t test_read(mem_handler_t *this, gb_address_t offset) {
  return memory[offset];
}

void test_mem_handler_init(void) {
  test_mem_handler.read = test_read;
  test_mem_handler.write = test_write;
  test_mem_handler.destroy = mem_handler_stack_destroy;
}

static void setup_memory_mapping(mmu_t *mmu) {
  as_handle_t idx = mmu_map_memory(mmu, 0x0000, 0xC000);
  test_mem_handler_init();
  mmu_register_mem_handler(mmu, &test_mem_handler, idx);
}

static void clean(cpu_t *cpu) {
  mmu_t *mmu = cpu->mmu;
  lcd_t *lcd = cpu->lcd;
  memset(cpu, 0, sizeof(cpu_t));
  cpu->mmu = mmu;
  mmu_clean(cpu->mmu);
  cpu->lcd = lcd;
}

static void do_test(test_t test, cpu_t *cpu) {
  test(cpu);
  clean(cpu);
}

/* The individual tests get defined by the TEST macro which creates
 * a symbol in the .testing_array section.
 * We traverse this section via two symbols defined by the linker script
 * LinkerScript.ld
 */

extern void (*__testing_array_start[]) (cpu_t *);
extern void (*__testing_array_end[]) (cpu_t *);

int main(void) {
  memset(memory, 0, sizeof(memory));
  mmu_t *mmu = mmu_new();
  lcd_t *lcd = lcd_new(mmu, memory + 0x8000, NULL);
  cpu_t *cpu = calloc(1, sizeof(cpu_t));
  cpu_init(cpu, mmu, lcd);
  setup_memory_mapping(mmu);

  const size_t size = __testing_array_end - __testing_array_start;
  for (size_t i = 0; i < size; ++i)
    do_test(__testing_array_start[i], cpu);

  fputs("All tests passed\n", stderr);
  cpu_delete(cpu);
}
