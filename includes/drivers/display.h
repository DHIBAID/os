#ifndef DISPLAY_H
#define DISPLAY_H

#include <stddef.h>
#include <stdint.h>

#include "lib/printf.h"
#include "lib/util.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_BUFFER ((volatile uint16_t*)0xb8000)

void scroll_screen();
void put_char_at(char c, size_t x, size_t y, uint8_t color);
void remove_char_at_cursor();

#endif  // DISPLAY_H
