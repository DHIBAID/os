void test_divide_by_zero() {
    int a = 1;
    int b = 0;      // Defined variables to shut up GCC warnings for division by 0
    int c = a / b;  // This will trigger IDT[0] = isr_divide_by_zero
    (void)c;        // Prevent unused variable warning
}