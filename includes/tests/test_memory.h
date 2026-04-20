#pragma once

#ifndef TEST_MEMORY_H
#define TEST_MEMORY_H

#include "drivers/memory.h"

// Allocates two physical frames, verifies frame accounting decrements/restores,
// and checks that the two frames are distinct non-null addresses.
void test_pmm_basic(void);

// Allocates two heap buffers with deterministic byte patterns, verifies no
// cross-buffer corruption, frees both, and checks heap stats return to baseline.
void test_heap_basic(void);

// Tests kcalloc zero-initialization, krealloc grow (preserves prefix),
// krealloc shrink (preserves prefix), and krealloc(ptr, 0) which frees.
void test_heap_calloc_realloc(void);

// Maps a physical frame into the current address space, verifies translation
// and read/write access through the mapped virtual address, then unmaps it.
void test_vmm_map_unmap(void);

// Creates a new process-style address space, maps/translates/unmaps a frame
// within it using the _in_space variants, then destroys the address space.
void test_vmm_address_space(void);

// Runs all memory tests in sequence and prints a pass/fail count summary.
void test_memory_all(void);

#endif	// TEST_MEMORY_H
