#pragma once

#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>

// First 4 MiB is reserved: 0x0-0xFFFFF for BIOS/IVT/VGA buffers, 0x100000
// onward for the kernel image (linked at 1 MiB). PMM skips this region on init.
#define PMM_RESERVED_BOOT_BYTES (4ULL * 1024ULL * 1024ULL)

// Maximum number of disjoint free regions the PMM can track simultaneously.
// Each reserve/unreserve call creates at most 2 new nodes (a split), so 1024
// nodes supports up to 512 such operations before the pool is exhausted.
// TODO: increase or make dynamic if the memory map is highly fragmented.
#define PMM_REGION_NODE_CAPACITY 1024ULL

// 8-byte alignment covers the natural alignment of all primitive types on
// x86-64, including pointers and uint64_t, without wasting significant space.
#define KHEAP_ALIGNMENT 8ULL

// 16 MiB hard cap prevents the heap from consuming all physical RAM.
// TODO: replace with dynamic expansion backed by vmm_map() / pmm_alloc_frame().
#define KHEAP_MAX_SIZE_BYTES (16ULL * 1024ULL * 1024ULL)

// x86-64 page tables use 9-bit index fields per level, giving 512 entries each.
#define VMM_ENTRIES_PER_TABLE 512ULL

// Bits 12-51 of a page table entry store the physical frame address.
// Bits 0-11 are control flags; bits 52-62 are available/reserved; bit 63 is NX.
#define VMM_ADDR_MASK 0x000FFFFFFFFFF000ULL

// For a 2 MiB page (signalled by VMM_PTE_PAGE_SIZE in a PDE), the base address
// occupies bits 21-51 (2^21 = 2 MiB). Bits 0-20 are the in-page offset.
#define VMM_LARGE_2M_ADDR_MASK 0x000FFFFFFFE00000ULL

// For a 1 GiB page (signalled by VMM_PTE_PAGE_SIZE in a PDPTE), the base
// address occupies bits 30-51 (2^30 = 1 GiB). Bits 0-29 are the in-page offset.
#define VMM_LARGE_1G_ADDR_MASK 0x000FFFFFC0000000ULL

// Standard x86-64 page table entry flag bits (Intel SDM Vol.3A Section 4.5).
#define VMM_PTE_PRESENT (1ULL << 0)	   // If clear, any access raises #PF.
#define VMM_PTE_WRITE (1ULL << 1)	   // Read-only if clear. CR0.WP extends this to ring 0.
#define VMM_PTE_USER (1ULL << 2)	   // Kernel-only if clear; ring 3 access raises #GP.
#define VMM_PTE_PAGE_SIZE (1ULL << 7)  // In PDE: 2 MiB leaf. In PDPTE: 1 GiB leaf.
#define VMM_PTE_GLOBAL (1ULL << 8)	   // TLB entry survives CR3 reloads. Use for kernel pages.
#define VMM_PTE_NX (1ULL << 63)		   // No-Execute. Only valid when EFER.NXE is set.

// EFER (Extended Feature Enable Register, MSR 0xC0000080): controls long mode
// and the NX feature. Must be read-modify-written via RDMSR/WRMSR.
#define MSR_EFER 0xC0000080U
#define EFER_NXE (1ULL << 11)  // Bit 11: enables the NX bit in page table entries.
#define CR0_WP (1ULL << 16)	   // Bit 16: Write Protect - ring 0 cannot write read-only pages.

#define PMM_FRAME_SIZE 4096ULL	// Physical frames are always 4 KiB.
#define VMM_PAGE_SIZE 4096ULL	// Virtual pages are always 4 KiB (matches frame size).

// A node in the PMM's sorted free-region linked list.
// Represents one contiguous range of free physical memory.
typedef struct PMMRegionNode {
	uint64_t base;	  // Physical start address (always frame-aligned).
	uint64_t length;  // Length in bytes (always a multiple of PMM_FRAME_SIZE).
	struct PMMRegionNode* next;
} PMMRegionNode;

// Intrusive header placed immediately before each heap allocation.
// The allocator walks this doubly-linked list to find free blocks (first-fit).
// The doubly-linked structure allows O(1) coalescing with the predecessor on free.
typedef struct HeapBlockHeader {
	size_t size;  // Usable bytes following this header (excludes header itself).
	uint8_t is_free;
	struct HeapBlockHeader* next;
	struct HeapBlockHeader* prev;
} HeapBlockHeader;

// VMM singleton. Populated once by vmm_init() from the bootloader's CR3.
typedef struct {
	uint64_t* kernel_pml4;	// Physical address of the kernel's PML4 table.
	int nx_enabled;			// 1 if EFER.NXE was successfully set.
	int initialized;
} VMMState;

// PMM singleton. Tracks aggregate frame counts.
// The actual free list lives in g_region_head (internal to pmm.c).
typedef struct {
	uint64_t total_memory_bytes;
	uint64_t total_frames;
	uint64_t free_frames;
	uint64_t region_count;	// Number of disjoint free regions currently tracked.
} PMM;

typedef struct {
	uint64_t total_bytes;
	uint64_t used_bytes;
	uint64_t free_bytes;
	uint64_t largest_free_block_bytes;
	uint64_t allocated_blocks;
	uint64_t free_blocks;
} HeapStats;

enum {
	VMM_FLAG_WRITE = 1ULL << 0,	   // Page is writable.
	VMM_FLAG_USER = 1ULL << 1,	   // Page is accessible from ring 3.
	VMM_FLAG_NO_EXEC = 1ULL << 2,  // Page is not executable (requires nx_enabled).
	VMM_FLAG_GLOBAL = 1ULL << 3,   // TLB entry not flushed on CR3 switch.
};

void pmm_init(uint64_t total_memory_bytes, void* metadata_storage);
void* pmm_alloc_frame(void);
void pmm_free_frame(void* frame_address);
void pmm_reserve_region(uint64_t base_address, uint64_t length);
void pmm_unreserve_region(uint64_t base_address, uint64_t length);

uint64_t pmm_total_frames(void);
uint64_t pmm_used_frames(void);
uint64_t pmm_free_frames(void);
uint64_t pmm_total_memory_bytes(void);

void vmm_init(void);
int vmm_map(uint64_t physical_addr, uint64_t virtual_addr, uint64_t flags);
int vmm_unmap(uint64_t virtual_addr);
uint64_t vmm_translate(uint64_t virtual_addr);
int vmm_map_range(uint64_t physical_addr, uint64_t virtual_addr, uint64_t size_bytes, uint64_t flags);

int vmm_map_in_space(uint64_t* pml4, uint64_t physical_addr, uint64_t virtual_addr, uint64_t flags);
int vmm_unmap_in_space(uint64_t* pml4, uint64_t virtual_addr);
uint64_t vmm_translate_in_space(uint64_t* pml4, uint64_t virtual_addr);
int vmm_map_range_in_space(uint64_t* pml4, uint64_t physical_addr, uint64_t virtual_addr, uint64_t size_bytes, uint64_t flags);

uint64_t* vmm_create_address_space(void);
void vmm_destroy_address_space(uint64_t* pml4);
void vmm_switch_address_space(uint64_t* pml4);
uint64_t* vmm_current_address_space(void);
uint64_t* vmm_kernel_address_space(void);
int vmm_nx_enabled(void);

void kheap_init(void* heap_start);
void* kmalloc(size_t size);
void* kcalloc(size_t count, size_t size);
void* krealloc(void* ptr, size_t new_size);
void kfree(void* ptr);

void kheap_get_stats(HeapStats* out_stats);
void meminfo(void);

void* memcpy(void* dest, const void* src, size_t n);
void* memset(void* dest, int c, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);

#endif	// MEMORY_H
