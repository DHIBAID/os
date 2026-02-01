#include "arch/idt.h"

void idt_init() {
    // Attach the divide by zero handler to interrupt vector 0
    idt_attach_handler(0, isr_divide_by_zero);
    // Attach the debug exception handler to interrupt vector 1
    idt_attach_handler(1, isr_debug_exception);
}

// IDT[0]
void divide_by_zero_handler() {
    kernel_panic("Divide by Zero Exception!");

    // Halt the CPU. Ideally we should kill the process if it tried to divide by zero in the kernel space.
    for (;;) __asm__ volatile("hlt");
}

// IDT[1]
void debug_exception_handler() {
    kernel_panic("Debug Exception!");

    // Debug registers DR0-DR3 contain breakpoint addresses.
    // Debug register DR6 tells us more about exception.
    // Debug register DR7 contains breakpoint control information.

    // Log contents of debug registers for debugging purposes
    registers_t regs;
    uint64_t dr0, dr1, dr2, dr3, dr6, dr7;
    __asm__ volatile(
        "mov %%dr0, %0\n"
        "mov %%dr1, %1\n"
        "mov %%dr2, %2\n"
        "mov %%dr3, %3\n"
        "mov %%dr6, %4\n"
        "mov %%dr7, %5\n"
        : "=r"(regs.DR0), "=r"(regs.DR1), "=r"(regs.DR2), "=r"(regs.DR3), "=r"(regs.DR6), "=r"(regs.DR7));

    print_str("\nDebug Registers:\n");
    print_str("DR0: 0x");
    print_hex(regs.DR0);
    print_str(" DR1: 0x");
    print_hex(regs.DR1);
    print_str(" DR2: 0x");
    print_hex(regs.DR2);
    print_str(" DR3: 0x");
    print_hex(regs.DR3);
    print_str("\nDR6: 0x");
    print_hex(regs.DR6);
    print_str(" DR7: 0x");
    print_hex(regs.DR7);
    print_str("\n");

    // Halt the CPU. Ideally we should allow debugging but we are too early here for that.
    for (;;) __asm__ volatile("hlt");
}
