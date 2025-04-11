#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stddef.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_BUFFER ((uint16_t *)0xb8000)

void scroll_screen();
void put_char_at(char c, size_t x, size_t y, uint8_t color);
static inline void outb(uint16_t port, uint8_t value);

#endif // DISPLAY_H
