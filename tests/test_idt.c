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
    // Set a breakpoint at address 0x0 to trigger a debug exception
    __asm__ volatile(
        "movq %0, %%dr0\n"
        "movq %1, %%dr7\n"  // Enable breakpoint 0 (execution)
        :
        : "r"(test_function), "r"((unsigned long)0x1));

    // Now call the function to trigger the breakpoint
    test_function(); // This should trigger IDT[1] = isr_debug_exception
}
