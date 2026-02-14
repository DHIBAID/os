#include "lib/util.h"

void sleep(int ms) {
    for (int i = 0; i < ms * 1000; i++) {
        __asm__ volatile("pause");
    }
}

// Helper functions for port I/O
void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void insl(uint16_t port, void* addr, uint32_t count) {
    __asm__ volatile("rep insl" : "+D"(addr), "+c"(count) : "d"(port) : "memory");
}