#include "drivers/display.h"

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void scroll_screen() {
    // Check if scrolling is needed
    int needs_scroll = 0;
    for (int x = 0; x < VGA_WIDTH; x++) {
        if (VGA_BUFFER[(VGA_HEIGHT - 1) * VGA_WIDTH + x] != (uint16_t)(' ' | (0x07 << 8))) {
            needs_scroll = 1;
            break;
        }
    }

    if (!needs_scroll) {
        return;  // No scrolling needed
    }

    for (int y = 1; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[(y - 1) * VGA_WIDTH + x] = VGA_BUFFER[y * VGA_WIDTH + x];
        }
    }

    // Clear the last line
    for (int x = 0; x < VGA_WIDTH; x++) {
        VGA_BUFFER[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = (uint16_t)(' ' | (0x07 << 8));  // Space with default color
    }
}

void put_char_at(char c, size_t x, size_t y, uint8_t color) {
    if (x >= VGA_WIDTH || y >= VGA_HEIGHT) {
        return;
    }

    VGA_BUFFER[y * VGA_WIDTH + x] = (uint16_t)(c | (color << 8));

    size_t cursor_position = y * VGA_WIDTH + x + 1;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(cursor_position & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((cursor_position >> 8) & 0xFF));
}

void remove_char_at_cursor() {
    outb(0x3D4, 0x0F);
    size_t cursor_position_low = inb(0x3D5);
    outb(0x3D4, 0x0E);
    size_t cursor_position_high = inb(0x3D5);
    size_t cursor_position = (cursor_position_high << 8) | cursor_position_low;

    if (cursor_position == 0) {
        return;
    }

    cursor_position--;

    size_t x1 = cursor_position % VGA_WIDTH;
    size_t y1 = cursor_position / VGA_WIDTH;

    VGA_BUFFER[y1 * VGA_WIDTH + x1] = 0;

    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(cursor_position & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((cursor_position >> 8) & 0xFF));

    // update the cursor position in printf.c
    col = x1;
    row = y1;
}