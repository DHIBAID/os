#include "./kernel.h"

void kernel_main()
{
    print_clear();
    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    print_str("Welcome to our 64-bit kernel!\n");

    print_str("/>: ");

    while (1)
    {
        char c = bios_get_char();
        char buffer[2] = {c, '\0'};
        update_input(c);
        c != '\n' ? print_str(buffer) : 0;
    }
}