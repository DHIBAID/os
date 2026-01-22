#include "kernel/kpanic.h"

void read_registers() {
    registers_t regs;
    __asm__ volatile(
        "mov %%rax, %0\n"
        "mov %%rbx, %1\n"
        "mov %%rcx, %2\n"
        "mov %%rdx, %3\n"
        "mov %%rsi, %4\n"
        "mov %%rdi, %5\n"
        "mov %%rbp, %6\n"
        "mov %%rsp, %7\n"
        "lea (%%rip), %%rax\n"
        "mov %%rax, %8\n"
        "pushfq\n"
        "pop %9\n"
        : "=m"(regs.RAX), "=m"(regs.RBX), "=m"(regs.RCX), "=m"(regs.RDX),
          "=m"(regs.RSI), "=m"(regs.RDI), "=m"(regs.RBP), "=m"(regs.RSP),
          "=m"(regs.RIP), "=m"(regs.RFLAGS));

    print_str("RAX: 0x");
    print_hex(regs.RAX);
    print_str(" RBX: 0x");
    print_hex(regs.RBX);
    print_str(" RCX: 0x");
    print_hex(regs.RCX);
    print_str(" RDX: 0x");
    print_hex(regs.RDX);
    print_str("\nRSI: 0x");
    print_hex(regs.RSI);
    print_str(" RDI: 0x");
    print_hex(regs.RDI);
    print_str(" RBP: 0x");
    print_hex(regs.RBP);
    print_str(" RSP: 0x");
    print_hex(regs.RSP);
    print_str("\nRIP: 0x");
    print_hex(regs.RIP);
    print_str(" RFLAGS: 0x");
    print_hex(regs.RFLAGS);
}

void kernel_panic(const char* message) {
    // Disable interrupts to prevent further system activity
    __asm__ volatile("cli");

    // Clear the screen and display the panic message
    print_clear();
    print_str("KERNEL PANIC: ");

    // Print the provided message
    print_str(message);

    // Print the registers
    print_str("\nRegisters:\n");
    read_registers();

    print_str("Stack trace:\n");
    // TODO: Implement stack trace by walking the stack frames
    // Walk the stack using RBP and print return addresses

    print_str("\nSystem halted.\n");

    // Halt the CPU
    for (;;) __asm__ volatile("hlt");
}