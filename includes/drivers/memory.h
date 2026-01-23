#pragma once

#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>

// Physical Memory Manager (PMM) - Frame allocator
void pmm_init(uint64_t mem_size, uint64_t* bitmap);
void* pmm_alloc_frame();
void pmm_free_frame(void* frame);

// Kernel Heap - Bump allocator
void kheap_init(void* heap_start);
void* kmalloc(size_t size);

// Memory manipulation functions
void* memcpy(void* dest, const void* src, size_t n);
void* memset(void* s, int c, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);

// Information functions
void meminfo();

// Constants
#define FRAME_SIZE 4096  // 4 KiB frames
#define BITMAP_BITS_PER_BYTE 8

static uint64_t* pmm_bitmap = 0;       // Bitmap to track frame allocation
static uint64_t pmm_total_frames = 0;  // Total number of frames
static uint64_t pmm_free_frames = 0;   // Number of free frames

#endif  // MEMORY_H