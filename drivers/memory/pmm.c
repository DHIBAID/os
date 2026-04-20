#include "drivers/memory.h"
#include "kernel/kpanic.h"
#include "lib/printf.h"

// PMM singleton. Tracks aggregate statistics only; the actual free list is g_region_head.
static PMM g_pmm = {0};

// The free-region list is kept sorted by base address to allow a single O(n)
// pass to coalesce adjacent regions (pmm_merge_regions). Nodes are drawn from
// a fixed-size pool so the PMM works before the heap is online.
static PMMRegionNode* g_region_head = (PMMRegionNode*)0;
static PMMRegionNode* g_region_spare = (PMMRegionNode*)0;  // Head of the pool's free-node stack.
static PMMRegionNode* g_region_pool = (PMMRegionNode*)0;
static uint64_t g_region_pool_capacity = 0;

// Fallback pool in the BSS. Used when pmm_init() receives metadata_storage == NULL,
// which happens during early boot before any heap or caller-supplied buffer exists.
static PMMRegionNode g_region_fallback_pool[PMM_REGION_NODE_CAPACITY];

static uint8_t* g_heap_start = (uint8_t*)0;
static uint8_t* g_heap_end = (uint8_t*)0;
static size_t g_heap_total_bytes = 0;
static HeapBlockHeader* g_heap_head = (HeapBlockHeader*)0;

// Round value up to the nearest multiple of align (align must be a power of 2).
// The bit trick (value + align - 1) & ~(align - 1) works because for any power
// of 2, ~(align - 1) is a mask that zeroes the lower log2(align) bits.
static uint64_t align_up_u64(uint64_t value, uint64_t align) {
	if (align == 0) return value;
	return (value + align - 1ULL) & ~(align - 1ULL);
}

static uint64_t align_down_u64(uint64_t value, uint64_t align) {
	if (align == 0) return value;
	return value & ~(align - 1ULL);
}

static uintptr_t align_up_ptr(uintptr_t value, uintptr_t align) {
	if (align == 0) return value;
	return (value + align - 1U) & ~(align - 1U);
}

static size_t align_up_size(size_t value, size_t align) {
	if (align == 0) return value;
	return (value + align - 1U) & ~(align - 1U);
}

static uint64_t min_u64(uint64_t a, uint64_t b) {
	return (a < b) ? a : b;
}

static uint64_t max_u64(uint64_t a, uint64_t b) {
	return (a > b) ? a : b;
}

static void pmm_reset_state(void) {
	g_pmm.total_memory_bytes = 0;
	g_pmm.total_frames = 0;
	g_pmm.free_frames = 0;
	g_pmm.region_count = 0;
	g_region_head = (PMMRegionNode*)0;
	g_region_spare = (PMMRegionNode*)0;
	g_region_pool = (PMMRegionNode*)0;
	g_region_pool_capacity = 0;
}

// Walk the free list to recount free frames and region entries.
// Called after every operation that modifies the list. O(n) in region count.
// TODO: maintain running counters to make stat queries O(1).
static void pmm_recompute_stats(void) {
	uint64_t free_frames = 0;
	uint64_t region_count = 0;
	PMMRegionNode* node = g_region_head;

	while (node) {
		free_frames += node->length / PMM_FRAME_SIZE;
		region_count++;
		node = node->next;
	}

	g_pmm.free_frames = free_frames;
	g_pmm.region_count = region_count;
}

// Initialize the node pool as a singly-linked stack so pmm_alloc_node and
// pmm_free_node are both O(1) push/pop operations.
static void pmm_init_pool(void* metadata_storage) {
	if (metadata_storage != (void*)0) {
		g_region_pool = (PMMRegionNode*)metadata_storage;
	} else {
		g_region_pool = g_region_fallback_pool;
	}

	g_region_pool_capacity = PMM_REGION_NODE_CAPACITY;
	g_region_spare = (PMMRegionNode*)0;

	for (uint64_t i = 0; i < g_region_pool_capacity; i++) {
		g_region_pool[i].next = g_region_spare;
		g_region_spare = &g_region_pool[i];
	}
}

static PMMRegionNode* pmm_alloc_node(void) {
	if (!g_region_spare) {
		kernel_panic("PMM region node pool exhausted");
		for (;;) __asm__ volatile("hlt");
	}

	PMMRegionNode* node = g_region_spare;
	g_region_spare = g_region_spare->next;
	node->base = 0;
	node->length = 0;
	node->next = (PMMRegionNode*)0;
	return node;
}

static void pmm_free_node(PMMRegionNode* node) {
	if (!node) return;
	node->next = g_region_spare;
	g_region_spare = node;
}

static uint64_t pmm_usable_bytes(void) {
	return g_pmm.total_frames * PMM_FRAME_SIZE;
}

// Clamp and align a [base_address, base_address+length) range to the boundaries
// of valid physical memory. Aligning the start down and end up ensures operations
// cover whole frames. Returns 0 if the range is entirely outside usable memory.
static int pmm_normalize_range(uint64_t base_address, uint64_t length, uint64_t* start, uint64_t* end) {
	if (length == 0 || !start || !end || g_pmm.total_frames == 0) {
		return 0;
	}

	uint64_t usable = pmm_usable_bytes();
	uint64_t s = align_down_u64(base_address, PMM_FRAME_SIZE);
	uint64_t max_addr = base_address + length;
	// Detect uint64_t wraparound (base + length overflowed).
	if (max_addr < base_address) {
		max_addr = usable;
	}
	uint64_t e = align_up_u64(max_addr, PMM_FRAME_SIZE);

	if (s >= usable) return 0;
	if (e > usable) e = usable;
	if (s >= e) return 0;

	*start = s;
	*end = e;
	return 1;
}

// Insert node into the free list in ascending order of base address.
// Keeping the list sorted makes pmm_merge_regions a single O(n) forward pass.
static void pmm_insert_region_sorted(PMMRegionNode* node) {
	if (!node) return;

	PMMRegionNode* prev = (PMMRegionNode*)0;
	PMMRegionNode* cur = g_region_head;

	while (cur && cur->base < node->base) {
		prev = cur;
		cur = cur->next;
	}

	node->next = cur;
	if (prev) {
		prev->next = node;
	} else {
		g_region_head = node;
	}
}

// Coalesce adjacent or overlapping free regions in a single forward pass.
// Works correctly because the list is sorted: once cur_end < next->base,
// no further merges are possible for the current node.
static void pmm_merge_regions(void) {
	PMMRegionNode* cur = g_region_head;
	while (cur && cur->next) {
		uint64_t cur_end = cur->base + cur->length;
		PMMRegionNode* next = cur->next;
		uint64_t next_end = next->base + next->length;

		if (cur_end >= next->base) {  // Overlap or adjacency - absorb next into cur.
			cur->length = max_u64(cur_end, next_end) - cur->base;
			cur->next = next->next;
			pmm_free_node(next);
			continue;  // Re-check cur against the new cur->next without advancing.
		}

		cur = cur->next;
	}
}

// Initialize the PMM with the total amount of physical memory.
// metadata_storage, if non-NULL, must point to a buffer of at least
// PMM_REGION_NODE_CAPACITY * sizeof(PMMRegionNode) bytes for the node pool.
// Pass NULL to use the fallback BSS pool.
void pmm_init(uint64_t total_memory_bytes, void* metadata_storage) {
	pmm_reset_state();

	if (total_memory_bytes < PMM_FRAME_SIZE) {
		return;
	}

	g_pmm.total_memory_bytes = total_memory_bytes;
	g_pmm.total_frames = total_memory_bytes / PMM_FRAME_SIZE;

	pmm_init_pool(metadata_storage);

	// Start the free list after the reserved boot region. The first 4 MiB
	// contains the IVT, BIOS data area, VGA buffer, and the kernel image.
	// Allocating from there would overwrite running code or firmware data.
	uint64_t usable = pmm_usable_bytes();
	uint64_t initial_start =
		align_up_u64(PMM_RESERVED_BOOT_BYTES, PMM_FRAME_SIZE);
	if (initial_start < usable) {
		PMMRegionNode* node = pmm_alloc_node();
		if (node) {
			node->base = initial_start;
			node->length = usable - initial_start;
			node->next = (PMMRegionNode*)0;
			g_region_head = node;
		}
	}

	// Reserve the physical memory occupied by the node pool itself so that
	// pmm_alloc_frame() cannot hand it out as a general-purpose frame.
	if (g_region_pool) {
		uint64_t pool_base = (uint64_t)(uintptr_t)g_region_pool;
		uint64_t pool_bytes = g_region_pool_capacity * sizeof(PMMRegionNode);
		pmm_reserve_region(pool_base, pool_bytes);
	}

	pmm_recompute_stats();
}

// Mark [base_address, base_address+length) as reserved (not allocatable).
// Handles 5 overlap cases between the reservation and each free region:
//   1. No overlap - skip.
//   2. Reservation completely covers region - remove region.
//   3. Reservation overlaps the left end - advance region base to end.
//   4. Reservation overlaps the right end - shrink region length.
//   5. Reservation is interior to region - split region into two nodes.
void pmm_reserve_region(uint64_t base_address, uint64_t length) {
	uint64_t start = 0;
	uint64_t end = 0;
	if (!pmm_normalize_range(base_address, length, &start, &end)) {
		return;
	}

	PMMRegionNode* prev = (PMMRegionNode*)0;
	PMMRegionNode* cur = g_region_head;

	while (cur && cur->base < end) {
		uint64_t cur_start = cur->base;
		uint64_t cur_end = cur->base + cur->length;

		if (cur_end <= start) {	 // Case 1: no overlap, region ends before reservation.
			prev = cur;
			cur = cur->next;
			continue;
		}

		if (start <= cur_start && end >= cur_end) {	 // Case 2: swallow entire region.
			PMMRegionNode* to_free = cur;
			cur = cur->next;
			if (prev) {
				prev->next = cur;
			} else {
				g_region_head = cur;
			}
			pmm_free_node(to_free);
			continue;
		}

		if (start <= cur_start && end < cur_end) {	// Case 3: trim left end.
			cur->base = end;
			cur->length = cur_end - end;
			break;
		}

		if (start > cur_start && end >= cur_end) {	// Case 4: trim right end.
			cur->length = start - cur_start;
			prev = cur;
			cur = cur->next;
			continue;
		}

		if (start > cur_start && end < cur_end) {  // Case 5: split into two.
			PMMRegionNode* right = pmm_alloc_node();
			if (right) {
				right->base = end;
				right->length = cur_end - end;
				right->next = cur->next;
				cur->next = right;
			}
			cur->length = start - cur_start;
			break;
		}
	}

	pmm_recompute_stats();
}

void pmm_unreserve_region(uint64_t base_address, uint64_t length) {
	uint64_t start = 0;
	uint64_t end = 0;
	if (!pmm_normalize_range(base_address, length, &start, &end)) {
		return;
	}

	PMMRegionNode* node = pmm_alloc_node();
	if (!node) {
		return;
	}

	node->base = start;
	node->length = end - start;
	node->next = (PMMRegionNode*)0;

	pmm_insert_region_sorted(node);
	pmm_merge_regions();
	pmm_recompute_stats();
}

// Always allocates from the head of the sorted list (lowest available address).
// This simple first-fit strategy keeps higher addresses free for large contiguous
// allocations and avoids spreading fragmentation across the address space.
void* pmm_alloc_frame(void) {
	if (!g_region_head) {
		return (void*)0;
	}

	PMMRegionNode* node = g_region_head;
	if (node->length < PMM_FRAME_SIZE || (node->base % PMM_FRAME_SIZE) != 0) {
		kernel_panic("PMM free-list corruption detected");
		for (;;) __asm__ volatile("hlt");
	}

	uint64_t frame = node->base;
	node->base += PMM_FRAME_SIZE;
	node->length -= PMM_FRAME_SIZE;

	if (node->length == 0) {
		g_region_head = node->next;
		pmm_free_node(node);
	}

	pmm_recompute_stats();
	return (void*)(uintptr_t)frame;
}

void pmm_free_frame(void* frame_address) {
	if (frame_address == (void*)0 || g_pmm.total_frames == 0) return;

	uintptr_t addr = (uintptr_t)frame_address;
	if ((addr % PMM_FRAME_SIZE) != 0U) return;
	if ((uint64_t)addr >= pmm_usable_bytes()) return;

	pmm_unreserve_region((uint64_t)addr, PMM_FRAME_SIZE);
}

uint64_t pmm_total_frames(void) {
	return g_pmm.total_frames;
}

uint64_t pmm_used_frames(void) {
	if (g_pmm.total_frames < g_pmm.free_frames) return 0;
	return g_pmm.total_frames - g_pmm.free_frames;
}

uint64_t pmm_free_frames(void) {
	return g_pmm.free_frames;
}

uint64_t pmm_total_memory_bytes(void) {
	return g_pmm.total_memory_bytes;
}

// Merge block with its immediate successor if the successor is also free.
// Absorbs the successor's header into block->size, removing the node from the list.
static void merge_with_next(HeapBlockHeader* block) {
	if (!block || !block->next || !block->next->is_free) {
		return;
	}

	HeapBlockHeader* next = block->next;
	// sizeof(HeapBlockHeader) accounts for the header we are eliminating.
	block->size += sizeof(HeapBlockHeader) + next->size;
	block->next = next->next;
	if (block->next) {
		block->next->prev = block;
	}
}

static int ptr_in_heap(const void* ptr) {
	const uint8_t* p = (const uint8_t*)ptr;
	if (!g_heap_start || !g_heap_end) return 0;
	return p >= g_heap_start && p < g_heap_end;
}

// Initialize the kernel heap starting at heap_start (will be aligned up to
// KHEAP_ALIGNMENT). The heap size is capped at KHEAP_MAX_SIZE_BYTES or the
// remaining physical memory, whichever is smaller. The region is reserved in
// the PMM so pmm_alloc_frame() cannot hand out overlapping frames.
void kheap_init(void* heap_start) {
	if (heap_start == (void*)0) {
		g_heap_start = (uint8_t*)0;
		g_heap_end = (uint8_t*)0;
		g_heap_total_bytes = 0;
		g_heap_head = (HeapBlockHeader*)0;
		return;
	}

	uintptr_t aligned_start =
		align_up_ptr((uintptr_t)heap_start, KHEAP_ALIGNMENT);
	uint64_t max_heap_bytes = KHEAP_MAX_SIZE_BYTES;
	if (g_pmm.total_memory_bytes > 0 &&
		aligned_start < g_pmm.total_memory_bytes) {
		uint64_t remaining = g_pmm.total_memory_bytes - (uint64_t)aligned_start;
		max_heap_bytes = min_u64(max_heap_bytes, remaining);
	}

	max_heap_bytes = align_up_u64(max_heap_bytes, KHEAP_ALIGNMENT);
	if (max_heap_bytes <= sizeof(HeapBlockHeader)) {
		g_heap_start = (uint8_t*)0;
		g_heap_end = (uint8_t*)0;
		g_heap_total_bytes = 0;
		g_heap_head = (HeapBlockHeader*)0;
		return;
	}

	g_heap_start = (uint8_t*)aligned_start;
	g_heap_total_bytes = (size_t)max_heap_bytes;
	g_heap_end = g_heap_start + g_heap_total_bytes;

	// Place a single free block spanning the entire heap region.
	g_heap_head = (HeapBlockHeader*)g_heap_start;
	g_heap_head->size = g_heap_total_bytes - sizeof(HeapBlockHeader);
	g_heap_head->is_free = 1;
	g_heap_head->next = (HeapBlockHeader*)0;
	g_heap_head->prev = (HeapBlockHeader*)0;

	if (g_pmm.total_memory_bytes > 0) {
		pmm_reserve_region((uint64_t)(uintptr_t)g_heap_start,
						   g_heap_total_bytes);
	}
}

// First-fit allocator. Walks the free list and takes the first block that fits.
// Rounds requested size up to KHEAP_ALIGNMENT before searching.
void* kmalloc(size_t size) {
	if (size == 0 || !g_heap_head) {
		return (void*)0;
	}

	size_t requested = align_up_size(size, KHEAP_ALIGNMENT);
	HeapBlockHeader* block = g_heap_head;

	while (block) {
		if (block->is_free && block->size >= requested) {
			// Only split the block if the remainder is large enough to hold a
			// full header plus at least one minimum-aligned allocation.
			// Splitting smaller remainders would create permanently unusable fragments.
			size_t split_threshold =
				requested + sizeof(HeapBlockHeader) + KHEAP_ALIGNMENT;
			if (block->size >= split_threshold) {
				uint8_t* split_addr = (uint8_t*)(block + 1) + requested;
				HeapBlockHeader* split = (HeapBlockHeader*)split_addr;
				split->size = block->size - requested - sizeof(HeapBlockHeader);
				split->is_free = 1;
				split->next = block->next;
				split->prev = block;
				if (split->next) {
					split->next->prev = split;
				}

				block->next = split;
				block->size = requested;
			}

			block->is_free = 0;
			return (void*)(block + 1);
		}

		block = block->next;
	}

	return (void*)0;
}

void* kcalloc(size_t count, size_t size) {
	if (count == 0 || size == 0) {
		return (void*)0;
	}

	// Guard against count * size wrapping around SIZE_MAX.
	if (count > ((size_t)-1) / size) {
		return (void*)0;
	}

	size_t total = count * size;
	uint8_t* ptr = (uint8_t*)kmalloc(total);
	if (!ptr) return (void*)0;

	for (size_t i = 0; i < total; i++) {
		ptr[i] = 0;
	}

	return ptr;
}

// Free ptr and coalesce with adjacent free blocks.
// Merging forward first, then backward, lets a single merge_with_next call on
// the predecessor absorb both the freed block and any already-merged successor.
void kfree(void* ptr) {
	if (!ptr || !g_heap_head || !ptr_in_heap(ptr)) {
		return;
	}

	HeapBlockHeader* block = ((HeapBlockHeader*)ptr) - 1;
	block->is_free = 1;

	// Merge forward first so a subsequent backward merge can absorb both sides.
	merge_with_next(block);
	if (block->prev && block->prev->is_free) {
		block = block->prev;
		merge_with_next(block);
	}
}

void* krealloc(void* ptr, size_t new_size) {
	if (ptr == (void*)0) {
		return kmalloc(new_size);
	}

	if (new_size == 0) {
		kfree(ptr);
		return (void*)0;
	}

	if (!ptr_in_heap(ptr)) {
		return (void*)0;
	}

	size_t requested = align_up_size(new_size, KHEAP_ALIGNMENT);
	HeapBlockHeader* block = ((HeapBlockHeader*)ptr) - 1;

	// Block already large enough - optionally split off the excess.
	if (block->size >= requested) {
		size_t split_threshold =
			requested + sizeof(HeapBlockHeader) + KHEAP_ALIGNMENT;
		if (block->size >= split_threshold) {
			uint8_t* split_addr = (uint8_t*)(block + 1) + requested;
			HeapBlockHeader* split = (HeapBlockHeader*)split_addr;
			split->size = block->size - requested - sizeof(HeapBlockHeader);
			split->is_free = 1;
			split->next = block->next;
			split->prev = block;
			if (split->next) {
				split->next->prev = split;
			}

			block->next = split;
			block->size = requested;
			merge_with_next(split);
		}
		return ptr;
	}

	// Try to grow in place by absorbing the immediately following free block.
	if (block->next && block->next->is_free) {
		size_t combined_size =
			block->size + sizeof(HeapBlockHeader) + block->next->size;
		if (combined_size >= requested) {
			merge_with_next(block);

			size_t split_threshold =
				requested + sizeof(HeapBlockHeader) + KHEAP_ALIGNMENT;
			if (block->size >= split_threshold) {
				uint8_t* split_addr = (uint8_t*)(block + 1) + requested;
				HeapBlockHeader* split = (HeapBlockHeader*)split_addr;
				split->size = block->size - requested - sizeof(HeapBlockHeader);
				split->is_free = 1;
				split->next = block->next;
				split->prev = block;
				if (split->next) {
					split->next->prev = split;
				}

				block->next = split;
				block->size = requested;
			}
			return ptr;
		}
	}

	// No in-place option - allocate a new block, copy, and free the old one.
	void* new_ptr = kmalloc(requested);
	if (!new_ptr) {
		return (void*)0;
	}

	uint8_t* src = (uint8_t*)ptr;
	uint8_t* dst = (uint8_t*)new_ptr;
	size_t copy_size = min_u64(block->size, requested);
	for (size_t i = 0; i < copy_size; i++) {
		dst[i] = src[i];
	}

	kfree(ptr);
	return new_ptr;
}

void kheap_get_stats(HeapStats* out_stats) {
	if (!out_stats) return;

	out_stats->total_bytes = g_heap_total_bytes;
	out_stats->used_bytes = 0;
	out_stats->free_bytes = 0;
	out_stats->largest_free_block_bytes = 0;
	out_stats->allocated_blocks = 0;
	out_stats->free_blocks = 0;

	HeapBlockHeader* block = g_heap_head;
	while (block) {
		if (block->is_free) {
			out_stats->free_blocks++;
			out_stats->free_bytes += block->size;
			out_stats->largest_free_block_bytes =
				max_u64(out_stats->largest_free_block_bytes, block->size);
		} else {
			out_stats->allocated_blocks++;
			out_stats->used_bytes += block->size;
		}
		block = block->next;
	}
}

void meminfo(void) {
	HeapStats heap_stats;
	kheap_get_stats(&heap_stats);

	print_str("PMM info:\n");
	print_str("  Total memory: ");
	print_dec(pmm_total_memory_bytes());
	print_str(" bytes\n");
	print_str("  Frame size: ");
	print_dec(PMM_FRAME_SIZE);
	print_str(" bytes\n");
	print_str("  Total frames: ");
	print_dec(pmm_total_frames());
	print_str("\n");
	print_str("  Used frames: ");
	print_dec(pmm_used_frames());
	print_str("\n");
	print_str("  Free frames: ");
	print_dec(pmm_free_frames());
	print_str("\n");
	print_str("  Free regions: ");
	print_dec(g_pmm.region_count);
	print_str("\n");

	print_str("Heap info:\n");
	print_str("  Heap size: ");
	print_dec(heap_stats.total_bytes);
	print_str(" bytes\n");
	print_str("  Used bytes: ");
	print_dec(heap_stats.used_bytes);
	print_str("\n");
	print_str("  Free bytes: ");
	print_dec(heap_stats.free_bytes);
	print_str("\n");
	print_str("  Allocated blocks: ");
	print_dec(heap_stats.allocated_blocks);
	print_str("\n");
	print_str("  Free blocks: ");
	print_dec(heap_stats.free_blocks);
	print_str("\n");
	print_str("  Largest free block: ");
	print_dec(heap_stats.largest_free_block_bytes);
	print_str(" bytes\n");
}
