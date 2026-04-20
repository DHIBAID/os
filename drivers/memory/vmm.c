#include "drivers/memory.h"

static VMMState g_vmm = {0};

// x86-64 uses a 4-level page table. A canonical virtual address is split as:
//   bits 47-39: PML4 index (9 bits)
//   bits 38-30: PDPT index (9 bits)
//   bits 29-21: PD index   (9 bits)
//   bits 20-12: PT index   (9 bits)
//   bits 11-0:  page offset (12 bits, 4 KiB)
// Bits 63-48 must be a sign extension of bit 47 (canonical form).

static uint64_t align_up_u64(uint64_t value, uint64_t align) {
	if (align == 0) return value;
	return (value + align - 1ULL) & ~(align - 1ULL);
}

static uint64_t align_down_u64(uint64_t value, uint64_t align) {
	if (align == 0) return value;
	return value & ~(align - 1ULL);
}

// Extract the 9-bit index for each page table level from a virtual address.
static uint64_t vmm_pml4_index(uint64_t vaddr) {
	return (vaddr >> 39) & 0x1FFULL;  // bits 47:39
}

static uint64_t vmm_pdpt_index(uint64_t vaddr) {
	return (vaddr >> 30) & 0x1FFULL;  // bits 38:30
}

static uint64_t vmm_pd_index(uint64_t vaddr) {
	return (vaddr >> 21) & 0x1FFULL;  // bits 29:21
}

static uint64_t vmm_pt_index(uint64_t vaddr) {
	return (vaddr >> 12) & 0x1FFULL;  // bits 20:12
}

static uint64_t read_cr0(void) {
	uint64_t value;
	__asm__ volatile("mov %%cr0, %0" : "=r"(value));
	return value;
}

static void write_cr0(uint64_t value) {
	__asm__ volatile("mov %0, %%cr0" : : "r"(value) : "memory");
}

// CR3 holds the physical address of the currently active PML4 table in bits 12-51.
// Bits 0-11 are PCD/PWT flags on some configurations; we mask them off when reading.
static uint64_t read_cr3_raw(void) {
	uint64_t value;
	__asm__ volatile("mov %%cr3, %0" : "=r"(value));
	return value;
}

// Writing CR3 flushes all non-global TLB entries - use sparingly to avoid
// unnecessary TLB shootdowns.
static void write_cr3_raw(uint64_t value) {
	__asm__ volatile("mov %0, %%cr3" : : "r"(value) : "memory");
}

// Read/write a Model-Specific Register via RDMSR/WRMSR.
// MSR address in ECX; 64-bit value split across EDX:EAX.
static uint64_t read_msr(uint32_t msr) {
	uint32_t low = 0;
	uint32_t high = 0;
	__asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
	return ((uint64_t)high << 32) | low;
}

static void write_msr(uint32_t msr, uint64_t value) {
	uint32_t low = (uint32_t)value;
	uint32_t high = (uint32_t)(value >> 32);
	__asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static void cpuid_ex(uint32_t leaf, uint32_t subleaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
	uint32_t a = 0;
	uint32_t b = 0;
	uint32_t c = 0;
	uint32_t d = 0;
	__asm__ volatile("cpuid"
					 : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
					 : "a"(leaf), "c"(subleaf));
	if (eax) *eax = a;
	if (ebx) *ebx = b;
	if (ecx) *ecx = c;
	if (edx) *edx = d;
}

// NX support is reported in CPUID extended leaf 0x80000001, EDX bit 20 (XD/NX).
// We first verify that leaf 0x80000001 is available by checking the max extended leaf.
static int cpu_supports_nx(void) {
	uint32_t max_extended_leaf = 0;
	cpuid_ex(0x80000000U, 0, &max_extended_leaf, 0, 0, 0);
	if (max_extended_leaf < 0x80000001U) {
		return 0;
	}

	uint32_t edx = 0;
	cpuid_ex(0x80000001U, 0, 0, 0, 0, &edx);
	return (edx & (1U << 20)) != 0U;  // bit 20: Execute Disable (NX).
}

// Enable CR0.WP and EFER.NXE to harden the kernel:
// - CR0.WP: prevents ring 0 from accidentally writing to read-only pages.
// - EFER.NXE: activates the NX bit in PTEs so data pages cannot be executed.
static void vmm_enable_protection_bits(void) {
	uint64_t cr0 = read_cr0();
	if ((cr0 & CR0_WP) == 0) {
		write_cr0(cr0 | CR0_WP);
	}

	if (cpu_supports_nx()) {
		uint64_t efer = read_msr(MSR_EFER);
		if ((efer & EFER_NXE) == 0) {
			write_msr(MSR_EFER, efer | EFER_NXE);
		}
		g_vmm.nx_enabled = 1;
	}
}

// Extract the physical table address from a page table entry.
// Bits 12-51 hold the address; bits 0-11 are control flags masked out.
static uint64_t* entry_to_table(uint64_t entry) {
	return (uint64_t*)(uintptr_t)(entry & VMM_ADDR_MASK);
}

static void zero_page_table(uint64_t* table) {
	for (uint64_t i = 0; i < VMM_ENTRIES_PER_TABLE; i++) {
		table[i] = 0;
	}
}

// Flush the TLB entry for a single virtual address. Cheaper than reloading
// CR3 (which flushes all non-global entries) when only one mapping changes.
static void invalidate_page(uint64_t vaddr) {
	__asm__ volatile("invlpg (%0)" : : "r"((void*)(uintptr_t)vaddr) : "memory");
}

// Build an intermediate page table entry (PML4E, PDPTE, or PDE pointing to a
// child table). Intermediate entries always carry WRITE so the kernel can
// update child tables without temporarily relaxing protection.
static uint64_t make_parent_entry(uint64_t table_addr, uint64_t flags) {
	uint64_t entry = (table_addr & VMM_ADDR_MASK) | VMM_PTE_PRESENT;
	if (flags & VMM_FLAG_WRITE) entry |= VMM_PTE_WRITE;
	if (flags & VMM_FLAG_USER) entry |= VMM_PTE_USER;
	if (g_vmm.nx_enabled && (flags & VMM_FLAG_NO_EXEC)) entry |= VMM_PTE_NX;
	return entry;
}

// Build a leaf PTE mapping one 4 KiB physical frame.
// GLOBAL keeps the TLB entry across CR3 reloads - use only for kernel mappings,
// never for user pages, as it would leak kernel addresses after a context switch.
static uint64_t make_leaf_entry(uint64_t physical_addr, uint64_t flags) {
	uint64_t entry = (physical_addr & VMM_ADDR_MASK) | VMM_PTE_PRESENT;
	if (flags & VMM_FLAG_WRITE) entry |= VMM_PTE_WRITE;
	if (flags & VMM_FLAG_USER) entry |= VMM_PTE_USER;
	if (flags & VMM_FLAG_GLOBAL) entry |= VMM_PTE_GLOBAL;
	if (g_vmm.nx_enabled && (flags & VMM_FLAG_NO_EXEC)) entry |= VMM_PTE_NX;
	return entry;
}

// The bootloader maps the first 1 GiB with 2 MiB pages. To map a single 4 KiB
// page within that range we must first split the 2 MiB PDE into 512 individual
// 4 KiB PTEs. A new PT frame is allocated from the PMM for this purpose.
static uint64_t* split_2m_page(uint64_t* pd, uint64_t pd_index) {
	uint64_t pde = pd[pd_index];
	if ((pde & VMM_PTE_PRESENT) == 0 || (pde & VMM_PTE_PAGE_SIZE) == 0) {
		return (uint64_t*)0;
	}

	void* pt_frame = pmm_alloc_frame();
	if (!pt_frame) {
		return (uint64_t*)0;
	}

	uint64_t* pt = (uint64_t*)pt_frame;
	uint64_t base = pde & VMM_LARGE_2M_ADDR_MASK;
	// Preserve the original page attributes on each 4 KiB sub-page.
	uint64_t template_flags =
		pde & (VMM_PTE_WRITE | VMM_PTE_USER | VMM_PTE_GLOBAL | VMM_PTE_NX);

	for (uint64_t i = 0; i < VMM_ENTRIES_PER_TABLE; i++) {
		pt[i] = (base + i * VMM_PAGE_SIZE) | VMM_PTE_PRESENT | template_flags;
	}

	// Replace the 2 MiB leaf PDE with a pointer to the new 4 KiB PT.
	// PAGE_SIZE bit is intentionally absent - this entry is now an intermediate node.
	pd[pd_index] = ((uint64_t)(uintptr_t)pt & VMM_ADDR_MASK) | VMM_PTE_PRESENT |
				   (pde & (VMM_PTE_WRITE | VMM_PTE_USER | VMM_PTE_NX));
	return pt;
}

// Walk the 4-level page table from PML4 down to the PT level for vaddr.
// If create is non-zero, missing intermediate tables are allocated from the PMM.
// USER flag is propagated up through all levels - the CPU checks it at every
// level of the walk, so omitting it from a parent blocks access even if the
// leaf PTE has it set.
// Returns 0 and sets *out_pt on success, -1 on failure.
static int walk_to_pt(uint64_t* pml4, uint64_t vaddr, int create, uint64_t flags, uint64_t** out_pt) {
	if (!pml4 || !out_pt) return -1;

	uint64_t pml4_i = vmm_pml4_index(vaddr);
	uint64_t pdpt_i = vmm_pdpt_index(vaddr);
	uint64_t pd_i = vmm_pd_index(vaddr);

	uint64_t pml4e = pml4[pml4_i];
	uint64_t* pdpt = (uint64_t*)0;

	if ((pml4e & VMM_PTE_PRESENT) == 0) {
		if (!create) return -1;
		void* frame = pmm_alloc_frame();
		if (!frame) return -1;
		pdpt = (uint64_t*)frame;
		zero_page_table(pdpt);
		pml4[pml4_i] = make_parent_entry((uint64_t)(uintptr_t)pdpt,
										 flags | VMM_FLAG_WRITE);
	} else {
		pdpt = entry_to_table(pml4e);
		if ((flags & VMM_FLAG_USER) != 0) {
			pml4[pml4_i] |= VMM_PTE_USER;
		}
	}

	uint64_t pdpte = pdpt[pdpt_i];
	uint64_t* pd = (uint64_t*)0;

	if ((pdpte & VMM_PTE_PRESENT) == 0) {
		if (!create) return -1;
		void* frame = pmm_alloc_frame();
		if (!frame) return -1;
		pd = (uint64_t*)frame;
		zero_page_table(pd);
		pdpt[pdpt_i] =
			make_parent_entry((uint64_t)(uintptr_t)pd, flags | VMM_FLAG_WRITE);
	} else {
		if ((pdpte & VMM_PTE_PAGE_SIZE) != 0) {
			// 1 GiB huge page - cannot descend further without splitting (not implemented).
			// TODO: implement 1 GiB page splitting if needed.
			return -1;
		}
		pd = entry_to_table(pdpte);
		if ((flags & VMM_FLAG_USER) != 0) {
			pdpt[pdpt_i] |= VMM_PTE_USER;
		}
	}

	uint64_t pde = pd[pd_i];
	uint64_t* pt = (uint64_t*)0;

	if ((pde & VMM_PTE_PRESENT) == 0) {
		if (!create) return -1;
		void* frame = pmm_alloc_frame();
		if (!frame) return -1;
		pt = (uint64_t*)frame;
		zero_page_table(pt);
		pd[pd_i] =
			make_parent_entry((uint64_t)(uintptr_t)pt, flags | VMM_FLAG_WRITE);
	} else {
		if ((pde & VMM_PTE_PAGE_SIZE) != 0) {
			if (!create) return -1;
			// 2 MiB page in the way - split into 512 x 4 KiB pages before proceeding.
			pt = split_2m_page(pd, pd_i);
			if (!pt) return -1;
		} else {
			pt = entry_to_table(pde);
		}
		if ((flags & VMM_FLAG_USER) != 0) {
			pd[pd_i] |= VMM_PTE_USER;
		}
	}

	*out_pt = pt;
	return 0;
}

void vmm_init(void) {
	if (g_vmm.initialized) {
		return;
	}

	// Read the PML4 physical address from CR3 (set up by the bootloader).
	g_vmm.kernel_pml4 = (uint64_t*)(uintptr_t)(read_cr3_raw() & VMM_ADDR_MASK);
	vmm_enable_protection_bits();

	// The bootloader identity-maps the first 1 GiB in PML4[0]. To make the
	// kernel addressable from the canonical upper half we alias PML4[511] to
	// the same PDPT as PML4[0]. PML4[511] covers the range 0xFFFFFF8000000000
	// to 0xFFFFFFFFFFFFFFFF (the top 512 GiB of virtual address space).
	// TODO: remove once the kernel is linked and mapped at a true higher-half address.
	if (g_vmm.kernel_pml4 && (g_vmm.kernel_pml4[511] & VMM_PTE_PRESENT) == 0 &&
		(g_vmm.kernel_pml4[0] & VMM_PTE_PRESENT) != 0) {
		g_vmm.kernel_pml4[511] = g_vmm.kernel_pml4[0];
		write_cr3_raw(read_cr3_raw());	// Flush TLB to make the alias visible.
	}

	g_vmm.initialized = 1;
}

int vmm_map_in_space(uint64_t* pml4, uint64_t physical_addr, uint64_t virtual_addr, uint64_t flags) {
	if (!pml4) return -1;
	if (virtual_addr == 0) return -1;  // Prevent mapping the null page.

	uint64_t phys = align_down_u64(physical_addr, VMM_PAGE_SIZE);
	uint64_t virt = align_down_u64(virtual_addr, VMM_PAGE_SIZE);

	uint64_t* pt = (uint64_t*)0;
	if (walk_to_pt(pml4, virt, 1, flags, &pt) != 0) {
		return -1;
	}

	uint64_t pte_i = vmm_pt_index(virt);
	pt[pte_i] = make_leaf_entry(phys, flags);

	// Only flush the TLB entry if we modified the currently active address space.
	// Modifying an inactive space needs no shootdown until it is switched to.
	if (pml4 == vmm_current_address_space()) {
		invalidate_page(virt);
	}

	return 0;
}

int vmm_unmap_in_space(uint64_t* pml4, uint64_t virtual_addr) {
	if (!pml4) return -1;

	uint64_t virt = align_down_u64(virtual_addr, VMM_PAGE_SIZE);
	uint64_t* pt = (uint64_t*)0;
	if (walk_to_pt(pml4, virt, 0, 0, &pt) != 0) {
		return -1;
	}

	uint64_t pte_i = vmm_pt_index(virt);
	if ((pt[pte_i] & VMM_PTE_PRESENT) == 0) {
		return -1;
	}

	pt[pte_i] = 0;
	if (pml4 == vmm_current_address_space()) {
		invalidate_page(virt);
	}
	return 0;
}

uint64_t vmm_translate_in_space(uint64_t* pml4, uint64_t virtual_addr) {
	if (!pml4) return 0;

	uint64_t pml4e = pml4[vmm_pml4_index(virtual_addr)];
	if ((pml4e & VMM_PTE_PRESENT) == 0) return 0;
	uint64_t* pdpt = entry_to_table(pml4e);

	uint64_t pdpte = pdpt[vmm_pdpt_index(virtual_addr)];
	if ((pdpte & VMM_PTE_PRESENT) == 0) return 0;
	if ((pdpte & VMM_PTE_PAGE_SIZE) != 0) {	 // 1 GiB leaf page.
		uint64_t base = pdpte & VMM_LARGE_1G_ADDR_MASK;
		return base + (virtual_addr & ((1ULL << 30) - 1ULL));
	}
	uint64_t* pd = entry_to_table(pdpte);

	uint64_t pde = pd[vmm_pd_index(virtual_addr)];
	if ((pde & VMM_PTE_PRESENT) == 0) return 0;
	if ((pde & VMM_PTE_PAGE_SIZE) != 0) {  // 2 MiB leaf page.
		uint64_t base = pde & VMM_LARGE_2M_ADDR_MASK;
		return base + (virtual_addr & ((1ULL << 21) - 1ULL));
	}
	uint64_t* pt = entry_to_table(pde);

	uint64_t pte = pt[vmm_pt_index(virtual_addr)];
	if ((pte & VMM_PTE_PRESENT) == 0) return 0;
	return (pte & VMM_ADDR_MASK) + (virtual_addr & (VMM_PAGE_SIZE - 1ULL));
}

int vmm_map(uint64_t physical_addr, uint64_t virtual_addr, uint64_t flags) {
	if (!g_vmm.initialized) {
		vmm_init();
	}
	return vmm_map_in_space(vmm_current_address_space(), physical_addr, virtual_addr, flags);
}

int vmm_unmap(uint64_t virtual_addr) {
	if (!g_vmm.initialized) {
		vmm_init();
	}
	return vmm_unmap_in_space(vmm_current_address_space(), virtual_addr);
}

uint64_t vmm_translate(uint64_t virtual_addr) {
	if (!g_vmm.initialized) {
		vmm_init();
	}
	return vmm_translate_in_space(vmm_current_address_space(), virtual_addr);
}

// Map a contiguous physical range to a contiguous virtual range one page at a time.
// On partial failure, all previously mapped pages in this call are unmapped to
// avoid leaving dangling mappings.
int vmm_map_range_in_space(uint64_t* pml4, uint64_t physical_addr, uint64_t virtual_addr, uint64_t size_bytes, uint64_t flags) {
	if (!pml4 || size_bytes == 0) {
		return -1;
	}

	uint64_t size = align_up_u64(size_bytes, VMM_PAGE_SIZE);
	uint64_t mapped = 0;

	while (mapped < size) {
		if (vmm_map_in_space(pml4, physical_addr + mapped, virtual_addr + mapped, flags) != 0) {
			while (mapped > 0) {
				mapped -= VMM_PAGE_SIZE;
				vmm_unmap_in_space(pml4, virtual_addr + mapped);
			}
			return -1;
		}
		mapped += VMM_PAGE_SIZE;
	}

	return 0;
}

int vmm_map_range(uint64_t physical_addr, uint64_t virtual_addr, uint64_t size_bytes, uint64_t flags) {
	if (!g_vmm.initialized) {
		vmm_init();
	}

	return vmm_map_range_in_space(vmm_current_address_space(), physical_addr, virtual_addr, size_bytes, flags);
}

// Recursively free all physical frames in a page table subtree.
// level 4 = PML4, 3 = PDPT, 2 = PD, 1 = PT (leaf entries point to data frames).
// Large pages (PAGE_SIZE bit set) are skipped - they are not individually owned
// by the VMM and must be freed separately by the caller if needed.
static void destroy_table_recursive(uint64_t* table, int level) {
	if (!table || level < 1) return;

	for (uint64_t i = 0; i < VMM_ENTRIES_PER_TABLE; i++) {
		uint64_t entry = table[i];
		if ((entry & VMM_PTE_PRESENT) == 0) {
			continue;
		}

		if (level == 1) {
			void* frame = (void*)(uintptr_t)(entry & VMM_ADDR_MASK);
			pmm_free_frame(frame);
			continue;
		}

		if ((entry & VMM_PTE_PAGE_SIZE) != 0) {
			continue;  // Large page - skip.
		}

		uint64_t* child = entry_to_table(entry);
		destroy_table_recursive(child, level - 1);
		pmm_free_frame(child);
	}
}

// Allocate a new PML4 and copy the kernel's upper-half entries (indices 256-511)
// into it. The lower half (0-255) is zeroed, reserved for user-space mappings.
// Sharing upper-half entries means kernel mappings are visible in every address
// space automatically - no per-space vmm_map() calls are needed for kernel pages.
uint64_t* vmm_create_address_space(void) {
	if (!g_vmm.initialized) {
		vmm_init();
	}

	void* frame = pmm_alloc_frame();
	if (!frame) {
		return (uint64_t*)0;
	}

	uint64_t* new_pml4 = (uint64_t*)frame;
	zero_page_table(new_pml4);

	if (g_vmm.kernel_pml4) {
		for (uint64_t i = 256; i < VMM_ENTRIES_PER_TABLE; i++) {
			new_pml4[i] = g_vmm.kernel_pml4[i];
		}
	}

	return new_pml4;
}

// Free all user-space page tables and their referenced frames for pml4.
// Only entries 0-255 (lower half / user space) are destroyed. Entries 256-511
// are shared with the kernel and must not be freed here.
void vmm_destroy_address_space(uint64_t* pml4) {
	if (!pml4) return;
	if (!g_vmm.initialized) {
		vmm_init();
	}
	if (pml4 == g_vmm.kernel_pml4) {
		return;	 // Never destroy the kernel's own address space.
	}

	for (uint64_t i = 0; i < 256; i++) {
		uint64_t entry = pml4[i];
		if ((entry & VMM_PTE_PRESENT) == 0 ||
			(entry & VMM_PTE_PAGE_SIZE) != 0) {
			continue;
		}

		uint64_t* pdpt = entry_to_table(entry);
		destroy_table_recursive(pdpt, 3);
		pmm_free_frame(pdpt);
	}

	pmm_free_frame(pml4);
}

void vmm_switch_address_space(uint64_t* pml4) {
	if (!pml4) return;
	write_cr3_raw((uint64_t)(uintptr_t)pml4 & VMM_ADDR_MASK);
}

// Read CR3 to find which PML4 is currently loaded. Used to determine whether
// a mapping change requires a TLB shootdown on the active address space.
uint64_t* vmm_current_address_space(void) {
	return (uint64_t*)(uintptr_t)(read_cr3_raw() & VMM_ADDR_MASK);
}

uint64_t* vmm_kernel_address_space(void) {
	if (!g_vmm.initialized) {
		vmm_init();
	}
	return g_vmm.kernel_pml4;
}

int vmm_nx_enabled(void) {
	if (!g_vmm.initialized) {
		vmm_init();
	}
	return g_vmm.nx_enabled;
}
