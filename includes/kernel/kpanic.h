#ifndef KPANIC_H
#define KPANIC_H

#include <stddef.h>

#include "drivers/disk.h"
#include "lib/printf.h"

typedef struct {
    // General purpose registers
    int64_t RAX, RBX, RCX, RDX;

    // Stack pointer, base pointer, and index registers
    int64_t RSI, RDI, RBP, RSP;

    // Instruction pointer and flags
    int64_t RIP, RFLAGS;

} registers_t;

void kernel_panic(const char* message);
void read_registers();

#endif