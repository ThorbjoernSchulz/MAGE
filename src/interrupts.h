#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct cpu cpu_t;

#define INT_VBLANK    0x1
#define INT_LCD_STAT  0x2
#define INT_TIMER     0x4
#define INT_SERIAL    0x8
#define INT_JOYPAD    0x10

void raise_interrupt(cpu_t *cpu, uint8_t interrupt);
uint8_t process_interrupts(cpu_t *cpu);
bool interrupts_ready(cpu_t *cpu);
