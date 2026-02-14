#pragma once

#ifndef IDT_H
#define IDT_H

#include <stdint.h>

#include "kernel/kpanic.h"
#include "lib/printf.h"

typedef struct debug_state {
    uint64_t dr0;
    uint64_t dr1;
    uint64_t dr2;
    uint64_t dr3;
    uint64_t dr6;
    uint64_t dr7;
} debug_state_t;

typedef struct interrupt_frame {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
} interrupt_frame_t;

struct idt_entry {
    uint16_t off_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t flags;
    uint16_t off_mid;
    uint32_t off_high;
    uint32_t zero;
} __attribute__((packed));

// The register structure for loading the IDT
struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

extern struct idt_entry idt[256];
extern struct idt_ptr idtr;

// Attach an interrupt handler to the IDT
void idt_attach_handler(int n, void* handler);

// Load the IDT into the CPU
void idt_load();

// Initialize the IDT with default handlers
void idt_init();

// Handler definitions from isrs.asm
void isr_divide_by_zero();
void isr_debug_exception();
void isr_nmi_exception();

#endif  // IDT_H