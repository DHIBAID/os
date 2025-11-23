#include "memory.h"

#include "../../interface/printf.h"

// Kernel Heap globals
static void* heap_current = 0;           // Current position in heap
static void* heap_start = 0;             // Start of heap
static size_t total_heap_allocated = 0;  // Total bytes allocated

void pmm_init(uint64_t mem_size, uint64_t* bitmap) {
    pmm_bitmap = bitmap;
    pmm_total_frames = mem_size / FRAME_SIZE;
    pmm_free_frames = pmm_total_frames;

    // Clear the bitmap (all frames initially free)
    uint64_t bitmap_size = (pmm_total_frames + 63) / 64;  // Round up to 64-bit chunks
    for (uint64_t i = 0; i < bitmap_size; i++) {
        pmm_bitmap[i] = 0;
    }
}

static void pmm_set_frame(uint64_t frame_num) {
    uint64_t index = frame_num / 64;
    uint64_t bit = frame_num % 64;
    pmm_bitmap[index] |= (1ULL << bit);
}

static void pmm_clear_frame(uint64_t frame_num) {
    uint64_t index = frame_num / 64;
    uint64_t bit = frame_num % 64;
    pmm_bitmap[index] &= ~(1ULL << bit);
}

static int pmm_test_frame(uint64_t frame_num) {
    uint64_t index = frame_num / 64;
    uint64_t bit = frame_num % 64;
    return (pmm_bitmap[index] & (1ULL << bit)) != 0;
}

void* pmm_alloc_frame() {
    if (pmm_free_frames == 0) {
        return 0;  // No free frames
    }

    // Find first free frame
    for (uint64_t frame = 0; frame < pmm_total_frames; frame++) {
        if (!pmm_test_frame(frame)) {
            pmm_set_frame(frame);
            pmm_free_frames--;
            return (void*)(frame * FRAME_SIZE);
        }
    }

    return 0;  // Should not reach here if pmm_free_frames > 0
}

void pmm_free_frame(void* frame) {
    uint64_t frame_addr = (uint64_t)frame;

    // Ensure frame is aligned to FRAME_SIZE
    if (frame_addr % FRAME_SIZE != 0) {
        return;  // Invalid frame address
    }

    uint64_t frame_num = frame_addr / FRAME_SIZE;

    // Ensure frame number is valid
    if (frame_num >= pmm_total_frames) {
        return;  // Invalid frame number
    }

    // Only free if currently allocated
    if (pmm_test_frame(frame_num)) {
        pmm_clear_frame(frame_num);
        pmm_free_frames++;
    }
}

void kheap_init(void* heap_start_addr) {
    heap_start = heap_start_addr;
    heap_current = heap_start_addr;
    total_heap_allocated = 0;
}

void* kmalloc(size_t size) {
    if (size == 0) {
        return 0;
    }

    // Align to 8-byte boundary for better performance
    size = (size + 7) & ~7;

    void* allocated = heap_current;
    heap_current = (void*)((uint64_t)heap_current + size);
    total_heap_allocated += size;  // Track total allocation

    return allocated;
}

void meminfo() {
    print_str("Memory Information:\n");
    print_str("- Total Frames: ");
    print_dec(pmm_total_frames);
    print_str("\n- Free Frames: ");
    print_dec(pmm_free_frames);
    print_str("\n");

    // Total allocated memory:
    uint64_t total_allocated = pmm_total_frames - pmm_free_frames;
    print_str("- Total Allocated Frames: ");
    print_dec(total_allocated);
    print_str("\n");

    // Memory usage percentage:
    uint64_t memory_usage = (total_allocated * 100) / pmm_total_frames;
    print_str("- Memory Usage: ");
    print_dec(memory_usage);
    print_str("%\n");

    // Heap information
    print_str("- Heap Allocated: ");
    print_dec(total_heap_allocated);
    print_str(" bytes\n");
    print_str("- Heap Used: ");
    print_dec((uint64_t)heap_current - (uint64_t)heap_start);
    print_str(" bytes\n");
}

void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;

    while (n--) {
        *d++ = *s++;
    }

    return dest;
}

void* memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;

    while (n--) {
        *p++ = (unsigned char)c;
    }

    return s;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;

    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }

    return 0;
}