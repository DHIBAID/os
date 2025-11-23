#include "kernel.h"

// Global variable definition
char* currentDirectory = "/";

// Forward declaration

// Example memory initialization function
void init_memory_management() {
    uint64_t total_memory = 1 * 1024 * 1024 * 1024;  // 1 GB for example

    static uint64_t pmm_bitmap_storage[16384];  // Enough for 1GB / 4KB frames
    // Initialize PMM
    pmm_init(total_memory, pmm_bitmap_storage);

    // Initialize kernel heap starting after a safe offset
    void* heap_start = (void*)0x400000;  // 4MB mark for example
    kheap_init(heap_start);
}

void kernel_main() {
    print_clear();
    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    print_str("Welcome to our 64-bit kernel!\n");

    // Setup memory management
    init_memory_management();

    print_str(strconcat(currentDirectory, ">: "));

    while (1) {
        char c = bios_get_char();
        char buffer[2] = {c, '\0'};
        update_input(c);
        c != '\n' ? print_str(buffer) : 0;
    }
}
