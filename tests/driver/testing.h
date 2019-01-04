#pragma once
#include <src/cpu.h>
#include <assert.h>
#include <stdio.h>

/* This macro creates a function NAME and moves a pointer to it into a special
 * section.
 * This can be used by the test driver to call all tests defined.
 */
#define TEST(NAME, ...) void NAME(cpu_t *cpu) {\
    fprintf(stderr, "Running %s...\n", __func__);\
    __VA_ARGS__;\
    fputs("Passed!\n", stderr);\
  }\
  void (*NAME##_pt)(cpu_t *) __attribute__((section(".testing_array"))) = NAME;


typedef void (*test_t)(cpu_t *cpu);

/* runs the cpu until a NOP was encountered */
void run(cpu_t *cpu);

