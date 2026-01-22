#include "lib/util.h"

void sleep(int ms) {
    for (int i = 0; i < ms * 1000; i++) {
        asm volatile("pause");
    }
}

inline void outw(uint16_t value, uint16_t port) {
    asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}
