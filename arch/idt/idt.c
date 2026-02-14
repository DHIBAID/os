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
void debug_exception_handler(interrupt_frame_t* frame, debug_state_t* state) {
    uint64_t cause = state->dr6;
    uint64_t saved_dr7 = 0;

    // Hardware breakpoint?
    if (cause & 0xF) {
        print_str("Hardware breakpoint hit\n");

        // Disable local enables temporarily
        uint64_t original_dr7 = state->dr7;
        state->dr7 &= ~0xFF;

        // Enable single-step
        frame->rflags |= (1 << 8);

        // Save original DR7 globally
        saved_dr7 = original_dr7;
    } else if (cause & (1 << 14)) {
        print_str("Single-step trap\n");

        // Clear TF
        frame->rflags &= ~(1 << 8);

        // Restore breakpoints
        state->dr7 = saved_dr7;
    }

    // Print debug register state for diagnostics
    print_str("Debug Register State:\n");
    print_str("DR0: ");
    print_hex(state->dr0);
    print_str("\n");
    print_str("DR1: ");
    print_hex(state->dr1);
    print_str("\n");
    print_str("DR2: ");
    print_hex(state->dr2);
    print_str("\n");
    print_str("DR3: ");
    print_hex(state->dr3);
    print_str("\n");
    print_str("DR6: ");
    print_hex(state->dr6);
    print_str("\n");
    print_str("DR7: ");
    print_hex(state->dr7);
    print_str("\n");

    state->dr6 = 0;
}
