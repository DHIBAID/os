#ifndef KERNEL_H
#define KERNEL_H

#include "arch/idt.h"
#include "drivers/display.h"
#include "drivers/keyboard.h"
#include "drivers/memory.h"
#include "kernel/kpanic.h"
#include "kernel/terminal.h"
#include "lib/printf.h"
#include "lib/string.h"
#include "lib/util.h"
#include "tests/test_idt.h"  // Include test headers as needed

extern char currentDirectory[256];

#endif  // KERNEL_H