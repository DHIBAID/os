#include "arch/idt.h"

// Building the IDT and IDTR structures and their handling, funciton created in isrs.asm

// Single definition lives in arch/idt.c — headers only declare externs
struct idt_entry idt[256];
struct idt_ptr idtr;

void idt_attach_handler(int n, void* handler) {
    uint64_t handler_addr = (uint64_t)handler;

    idt[n].off_low = handler_addr & 0xFFFF;
    idt[n].selector = 0x08;  // Kernel code segment
    idt[n].ist = 0;          // No IST
    idt[n].flags = 0x8E;     // Present, DPL=0, Type=14 (32-bit interrupt gate)
    idt[n].off_mid = (handler_addr >> 16) & 0xFFFF;
    idt[n].off_high = (handler_addr >> 32) & 0xFFFFFFFF;
    idt[n].zero = 0;
}

void idt_load() {
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint64_t)&idt;

    // Load into IDTR register the memory address of "idtr"
    __asm__ volatile("lidt %0" : : "m"(idtr));
}

