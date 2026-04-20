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
		: "=m"(regs.RAX), "=m"(regs.RBX), "=m"(regs.RCX), "=m"(regs.RDX), "=m"(regs.RSI), "=m"(regs.RDI), "=m"(regs.RBP), "=m"(regs.RSP), "=m"(regs.RIP), "=m"(regs.RFLAGS));

	print_str("RAX: ");
	print_hex(regs.RAX);
	print_str(" RBX: ");
	print_hex(regs.RBX);
	print_str(" RCX: ");
	print_hex(regs.RCX);
	print_str(" RDX: ");
	print_hex(regs.RDX);
	print_str("\nRSI: ");
	print_hex(regs.RSI);
	print_str(" RDI: ");
	print_hex(regs.RDI);
	print_str(" RBP: ");
	print_hex(regs.RBP);
	print_str(" RSP: ");
	print_hex(regs.RSP);
	print_str("\nRIP: ");
	print_hex(regs.RIP);
	print_str(" RFLAGS: ");
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

	print_str("\nStack trace:\n");

	// Walk the stack using RBP and print return addresses
	uint64_t* rbp;
	__asm__ volatile("mov %%rbp, %0" : "=r"(rbp));

	for (int i = 0; i < 10; i++) {
		if (!rbp || !*rbp)
			break;

		uint64_t return_address = *(rbp + 1);

		// Adjust to get call site instead of return site
		uint64_t call_site = return_address - 1;

		print_str(" ");
		print_hex(call_site);
		print_str("\n");

		rbp = (uint64_t*)(*rbp);
	}
}