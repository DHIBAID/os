#include "../../interface/printf.h"
#include "../x86_64/drivers/keyboard.h"
#include "../x86_64/drivers/display.h"

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
        print_str(buffer);
    }
}