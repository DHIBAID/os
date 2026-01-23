#include "arch/idt.h"

void idt_init() {
    // Attach the divide by zero handler to interrupt vector 0
    idt_attach_handler(0, isr_divide_by_zero);
}

void divide_by_zero_handler() {
    kernel_panic("Divide by Zero Exception!");

    // Halt the CPU. Ideally we should kill the process if it tried to divide by zero in the kernel space.
    for (;;) __asm__ volatile("hlt");
}
