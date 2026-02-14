#include "tests/test_idt.h"

void test_divide_by_zero() {
    int a = 1;
    int b = 0;      // Defined variables to shut up GCC warnings for division by 0
    int c = a / b;  // This will trigger IDT[0] = isr_divide_by_zero
    (void)c;        // Prevent unused variable warning
}

// This function is intentionally left empty.
// It serves as a target for the debug exception test.
void test_function() {}

void test_debug_exception() {
    uint64_t dr7 = 0;
    dr7 |= (1ULL << 0);      // enable DR0 execution breakpoint

    __asm__ volatile(
        "mov %0, %%dr0\n"
        "mov %1, %%dr7\n"
        :
        : "r"(test_function), "r"(dr7)
    );

    test_function();  // triggers #DB
}

void test_nmi_exception() {
    // Modern x86 systems don't allow software to trigger NMIs directly, so we can't test this in a user-space test.
    // However, we can at least verify that the NMI handler is correctly registered in the IDT by checking the IDT entries in the kernel code.
    // If using a virtual machine, you can configure the VM to trigger an NMI.
    // For QEMU, you can use the QEMU monitor command: `nmi` to trigger an NMI and observe the kernel's response.
}
