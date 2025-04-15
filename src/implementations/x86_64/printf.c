#include "../../interface/printf.h"
#include "./drivers/display.h"

size_t col = 0;
size_t row = 0;

static uint8_t color = PRINT_COLOR_WHITE | (PRINT_COLOR_BLACK << 4);

void clear_row(size_t row)
{
    for (size_t col = 0; col < VGA_WIDTH; col++) {
        put_char_at(' ', col, row, color);
    }
}

void print_clear()
{
    for (size_t i = 0; i < VGA_HEIGHT; i++) {
        clear_row(i);
    }
    col = 0;
    row = 0;
}

void print_newline()
{
    col = 0;

    if (row < VGA_HEIGHT - 1) {
        row++;
        return;
    }

    scroll_screen();
    row = VGA_HEIGHT - 1;
}

void print_char(char character)
{
    if (character == '\n') {
        print_newline();
        return;
    }

    if (col >= VGA_WIDTH) {
        print_newline();
    }

    put_char_at(character, col, row, color);
    col++;
}

void print_str(char *str)
{
    for (size_t i = 0; str[i] != '\0'; i++) {
        print_char(str[i]);
    }
}

void print_set_color(uint8_t foreground, uint8_t background)
{
    color = foreground |  (background << 4);
}