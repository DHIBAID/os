#include "tests/test_idt.h"

#include "arch/idt.h"
#include "lib/printf.h"

void test_divide_by_zero() {
	int a = 1;
	int b = 0;		// Defined variables to shut up GCC warnings for division by 0
	int c = a / b;	// This will trigger IDT[0] = isr_divide_by_zero
	(void)c;		// Prevent unused variable warning
}

// This function is intentionally left empty.
// It serves as a target for the debug exception test.
void test_function() {
}

void test_debug_exception() {
	uint64_t dr7 = 0;
	dr7 |= (1ULL << 0);	 // enable DR0 execution breakpoint

	__asm__ volatile(
		"mov %0, %%dr0\n"
		"mov %1, %%dr7\n"
		:
		: "r"(test_function), "r"(dr7));

	test_function();  // triggers #DB
}

void test_nmi_exception() {
	// Modern x86 systems don't allow software to trigger NMIs directly, so we can't test this in a user-space test.
	// However, we can at least verify that the NMI handler is correctly registered in the IDT by checking the IDT entries in the kernel code.
	// If using a virtual machine, you can configure the VM to trigger an NMI.
	// For QEMU, you can use the QEMU monitor command: `nmi` to trigger an NMI and observe the kernel's response.
}

void test_general_protection_fault() {
	// Load an invalid segment selector to force #GP.
	__asm__ volatile(
		"mov $0x28, %%ax\n"
		"mov %%ax, %%ds\n"	// This will trigger IDT[13] = isr_general_protection_fault
		: : : "ax");

	// NOTE: In 64-bit mode, segment registers are mostly ignored for regular memory access
	//       but the CPU still validates the selector when you explicitly write to DS.
	//       So this trick still works even in long mode.

	// Why ax and not rax?
	// Segment registers like DS can only be loaded from a 16-bit register.
	// You can't do mov %%rax, %%ds. The CPU doesn't allow it.
	// So ax (the lower 16 bits of rax) is the right tool here.
	// Clobbering "ax" is sufficient because the compiler understands that touching ax also affects the lower bits of rax.
}

void test_page_fault() {
	// Boot code maps only the low 1 GiB with 2 MiB pages; this address is intentionally outside that range.
	volatile uint64_t* bad_ptr = (volatile uint64_t*)0x50000000;  // 0x50000000 = 1.25 GiB,
																  //  assigning to a volatile pointer to prevent compiler optimizations that might remove the access.
	volatile uint64_t value = *bad_ptr;							  // This will trigger IDT[14] = isr_page_fault
	(void)value;												  // Prevent unused variable warning
}

void test_double_fault() {
	// This is a 2 stage escalation
	// Stage 1: Move RSP into unmapped memory and then trigger #PF.
	//          Same logic as test_page_fault, assignment does not cause the #PF, only the dereference does.
	//          Once the movq tries to push the interrupt frame for the #PF onto the stack at the unmapped RSP,
	//          It will cause another fault because the stack is not accessible, escalating into a #DF.

	// Stage 2: The #PF handler will attempt to push the interrupt frame onto the stack,
	//          but since RSP points to unmapped memory, it will cause another fault while trying to push,
	//          escalating into a #DF.

	__asm__ volatile(
		"mov $0x50000000, %%rax\n"	// Stage 1 Prep: Load an unmapped address into RAX
		"mov %%rax, %%rsp\n"		// Stage 1 Prep: Move RSP to unmapped memory
		"movq $0, (%%rsp)\n"		// Stage 1 Trigger: This instruction will cause a #PF,
									//                  and when the CPU tries to push the #PF frame onto the stack at the unmapped RSP,
									//                  it will cause another fault, escalating into a #DF.
		: : : "rax", "memory");		// Clobber rax and memory to prevent compiler optimizations that might interfere with the test
}
