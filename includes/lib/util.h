#pragma once

#include <stddef.h>
#include <stdint.h>

void sleep(int ms);

// Helper functions for port I/O
void outb(uint16_t port, uint8_t val);
uint8_t inb(uint16_t port);
void outw(uint16_t port, uint16_t val);
uint16_t inw(uint16_t port);
void insl(uint16_t port, void* addr, uint32_t count);