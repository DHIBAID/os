#include "arch/idt.h"

// Building the IDT and IDTR structures and their handling, funciton created in
// isrs.asm

#define DF_IST_INDEX 1
#define DF_IST_STACK_SIZE 4096

struct __attribute__((packed)) tss64 {
	uint32_t reserved0;
	uint64_t rsp0;
	uint64_t rsp1;
	uint64_t rsp2;
	uint64_t reserved1;
	uint64_t ist1;
	uint64_t ist2;
	uint64_t ist3;
	uint64_t ist4;
	uint64_t ist5;
	uint64_t ist6;
	uint64_t ist7;
	uint64_t reserved2;
	uint16_t reserved3;
	uint16_t io_map_base;
};

struct __attribute__((packed)) tss_descriptor {
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_mid;
	uint8_t access;
	uint8_t granularity;
	uint8_t base_high;
	uint32_t base_upper;
	uint32_t reserved;
};

static uint64_t gdt64_runtime[7];
static struct tss64 kernel_tss;
static struct tss_descriptor kernel_tss_desc;
static uint8_t df_ist_stack[DF_IST_STACK_SIZE] __attribute__((aligned(16)));
static int gdt_tss_ready = 0;

static inline uint64_t build_code_segment_descriptor(uint8_t dpl) {
	return (0xAULL << 40) | (1ULL << 44) | ((uint64_t)(dpl & 0x3U) << 45) |
		   (1ULL << 47) | (1ULL << 53);
}

static inline uint64_t build_data_segment_descriptor(uint8_t dpl) {
	return (0x2ULL << 40) | (1ULL << 44) | ((uint64_t)(dpl & 0x3U) << 45) |
		   (1ULL << 47);
}

static void fill_tss_descriptor(struct tss_descriptor* desc, struct tss64* tss) {
	uint64_t base = (uint64_t)tss;
	uint32_t limit = sizeof(struct tss64) - 1;

	desc->limit_low = limit & 0xFFFF;
	desc->base_low = base & 0xFFFF;
	desc->base_mid = (base >> 16) & 0xFF;
	desc->access = 0x89;  // Present | Type=9 (available 64-bit TSS)
	desc->granularity = (limit >> 16) & 0x0F;
	desc->base_high = (base >> 24) & 0xFF;
	desc->base_upper = (base >> 32) & 0xFFFFFFFF;
	desc->reserved = 0;
}

void gdt_tss_init() {
	struct {
		uint16_t limit;
		uint64_t base;
	} __attribute__((packed)) gdtr;

	if (gdt_tss_ready) {
		return;
	}

	kernel_tss = (struct tss64){0};
	kernel_tss.io_map_base = sizeof(struct tss64);
	kernel_tss.ist1 = (uint64_t)(df_ist_stack + DF_IST_STACK_SIZE);

	gdt64_runtime[0] = 0;
	gdt64_runtime[1] = build_code_segment_descriptor(0);
	gdt64_runtime[2] = build_data_segment_descriptor(0);
	gdt64_runtime[3] = build_data_segment_descriptor(3);
	gdt64_runtime[4] = build_code_segment_descriptor(3);

	fill_tss_descriptor(&kernel_tss_desc, &kernel_tss);
	gdt64_runtime[5] = *(uint64_t*)&kernel_tss_desc;
	gdt64_runtime[6] =
		((uint64_t)kernel_tss_desc.base_upper << 32) | kernel_tss_desc.reserved;

	gdtr.limit = sizeof(gdt64_runtime) - 1;
	gdtr.base = (uint64_t)&gdt64_runtime;

	__asm__ volatile("lgdt %0" : : "m"(gdtr));
	__asm__ volatile(
		"mov %0, %%ax\n\t"
		"mov %%ax, %%ds\n\t"
		"mov %%ax, %%es\n\t"
		"mov %%ax, %%ss\n\t"
		"mov %%ax, %%fs\n\t"
		"mov %%ax, %%gs\n\t"
		:
		: "r"((uint16_t)GDT_SELECTOR_KERNEL_DATA)
		: "ax", "memory");
	__asm__ volatile("ltr %0" : : "r"((uint16_t)GDT_SELECTOR_TSS));

	gdt_tss_ready = 1;
}

// Return functions for constants
uint16_t gdt_kernel_code_selector(void) {
	return GDT_SELECTOR_KERNEL_CODE;
}

uint16_t gdt_kernel_data_selector(void) {
	return GDT_SELECTOR_KERNEL_DATA;
}

uint16_t gdt_user_code_selector(void) {
	return (uint16_t)(GDT_SELECTOR_USER_CODE | 0x3U);
}

uint16_t gdt_user_data_selector(void) {
	return (uint16_t)(GDT_SELECTOR_USER_DATA | 0x3U);
}

// Single definition lives in arch/idt.c — headers only declare externs
struct idt_entry idt[256];
struct idt_ptr idtr;

void idt_attach_handler(int n, void* handler) {
	idt_attach_handler_ist(n, handler, 0);
}

void idt_attach_handler_ist(int n, void* handler, uint8_t ist_index) {
	uint64_t handler_addr = (uint64_t)handler;

	idt[n].off_low = handler_addr & 0xFFFF;
	idt[n].selector = GDT_SELECTOR_KERNEL_CODE;
	idt[n].ist = ist_index & 0x7;
	idt[n].flags = 0x8E;  // Present, DPL=0, Type=14 (32-bit interrupt gate)
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
