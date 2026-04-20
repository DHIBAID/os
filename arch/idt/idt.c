#include "arch/idt.h"

#include "lib/string.h"

// IST index 1 refers to the first Interrupt Stack Table entry in the TSS.
// The TSS provides up to 7 dedicated stacks (IST1-IST7). IST1 is used for #DF
// because a double fault can occur when RSP is corrupted or points to unmapped
// memory. Without a dedicated stack, the CPU would fault again trying to push
// the exception frame, causing a triple fault and immediate reset.
#define DF_IST_INDEX 1

void idt_init() {
	// Attach the divide by zero handler to interrupt vector 0
	idt_attach_handler(0, isr_divide_by_zero);
	// Attach the debug exception handler to interrupt vector 1
	idt_attach_handler(1, isr_debug_exception);
	// Attach the NMI handler to interrupt vector 2
	idt_attach_handler(2, isr_nmi_exception);
	// Double fault uses IST1 - a dedicated stack immune to RSP corruption.
	idt_attach_handler_ist(8, isr_double_fault, DF_IST_INDEX);
	// Attach the General Protection Fault handler to interrupt vector 13.
	idt_attach_handler(13, isr_general_protection_fault);
	// Attach the Page Fault handler to interrupt vector 14.
	idt_attach_handler(14, isr_page_fault);
}

// IDT[0]
void divide_by_zero_handler() {
	kernel_panic("Divide by Zero Exception!");

	// Halt the CPU. Ideally we should kill the process if it tried to divide by zero in the kernel space.
	for (;;) __asm__ volatile("hlt");
}

// IDT[1]
// DR6 is the debug status register. Its bits indicate what triggered the exception:
//   bits 0-3 (B0-B3): breakpoint condition met for DR0-DR3 respectively.
//   bit 14 (BS): single-step trap (Trap Flag was set in RFLAGS).
// DR7 is the debug control register. Its low 8 bits are local/global enable
// bits for each of the 4 hardware breakpoint registers.
void debug_exception_handler(interrupt_frame_t* frame, debug_state_t* state) {
	uint64_t cause = state->dr6;
	// Persists across handler invocations to restore DR7 after the single-step
	// sequence completes. A local variable would be lost between the two calls.
	static uint64_t saved_dr7 = 0;

	if (cause & 0xF) {	// bits 0-3: one or more of DR0-DR3 triggered.
		print_str("Hardware breakpoint hit\n");

		// Clear the local enable bits (bits 0-7 of DR7) to prevent the same
		// breakpoint from immediately re-triggering while we single-step past it.
		uint64_t original_dr7 = state->dr7;
		state->dr7 &= ~0xFF;

		// Set the Trap Flag (bit 8 in RFLAGS) to fire another #DB after the
		// next instruction completes, at which point we restore DR7.
		frame->rflags |= (1 << 8);

		// Save original DR7 globally
		saved_dr7 = original_dr7;
	} else if (cause & (1 << 14)) {	 // bit 14: BS - single-step trap.
		print_str("Single-step trap\n");

		// Clear TF - we only wanted one step past the breakpoint.
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

	// Clear DR6 so the next exception gets a clean status register.
	// Leaving stale bits set can cause misidentification of the trigger on the next #DB.
	state->dr6 = 0;
}

// IDT[2]
void nmi_exception_handler(interrupt_frame_t* frame) {
	(void)frame;
	kernel_panic("Non-Maskable Interrupt (NMI) Exception!");

	// Halt the CPU
	for (;;) __asm__ volatile("hlt");
}

// IDT[8]
// The #DF error code is architecturally defined as always 0 (Intel SDM Vol.3A 6.15).
// We accept and display it only to keep the handler signature consistent with other
// error-code handlers - there is no selector or fault address encoded here.
void double_fault_handler(interrupt_frame_t* frame, uint64_t error_code) {
	(void)frame;

	char hex_buf[19];  // "0x" + 16 hex digits + '\0'
	kernel_panic(strconcat("Double Fault Exception!\nError code: ", u64_to_hex(error_code, hex_buf)));

	for (;;) __asm__ volatile("hlt");
}

// IDT[13]
// The #GP error code encodes the segment selector that caused the fault, or 0
// if not selector-related (e.g. a canonical-address violation or accessing a
// kernel-only page from ring 3).
void general_protection_fault_handler(interrupt_frame_t* frame, uint64_t error_code) {
	(void)frame;

	char hex_buf[19];  // "0x" + 16 hex digits + '\0'
	kernel_panic(strconcat("General Protection Fault Exception!\nError code: ", u64_to_hex(error_code, hex_buf)));

	for (;;) __asm__ volatile("hlt");
}

// IDT[14]
// The #PF error code bits:
//   bit 0 (P)  - 0 = not-present fault, 1 = protection violation.
//   bit 1 (W)  - 0 = read, 1 = write.
//   bit 2 (U)  - 0 = supervisor (ring 0), 1 = user (ring 3).
//   bit 4 (I)  - 1 = instruction fetch (NX violation; requires EFER.NXE).
// CR2 holds the faulting virtual address at the time of the fault.
// TODO: use error_code and cr2 to implement demand paging instead of panicking.
void page_fault_handler(interrupt_frame_t* frame, uint64_t error_code, uint64_t cr2) {
	(void)frame;
	(void)cr2;	// Not yet used - will be needed for demand paging.

	char hex_buf[19];  // "0x" + 16 hex digits + '\0'
	kernel_panic(strconcat("Page Fault Exception!\nError code: ", u64_to_hex(error_code, hex_buf)));

	for (;;) __asm__ volatile("hlt");
}
