#include "keyboard.h"
#include "display.h"
#include "../../kernel/terminal.h"

#define BACKSPACE_SCANCODE 0x0E

static int usb_read(uint8_t *buffer, size_t size);

char bios_get_char()
{
    char c = 0;
    while ((c = read_from_usb_keyboard()) == 0)
    {
        asm volatile("pause");
    }
    return c;
}

char read_from_usb_keyboard()
{
    static uint8_t last_scancode = 0;
    static uint8_t key_released = 1;
    uint8_t buffer[USB_KEYBOARD_BUFFER_SIZE];
    char key_char = 0;

    if (usb_read(buffer, USB_KEYBOARD_BUFFER_SIZE) > 0)
    {
        uint8_t current_scancode = buffer[2];

        if (current_scancode != 0)
        {
            if (current_scancode != last_scancode || key_released)
            {
                if (current_scancode == BACKSPACE_SCANCODE)
                {
                    remove_char_at_cursor();
                    key_char = 0;
                    update_input('\b');
                }
                else
                {
                    key_char = (char)buffer[0];
                }

                last_scancode = current_scancode;
                key_released = 0;
            }
        }
        else
        {
            key_released = 1;
            last_scancode = 0;
        }
    }

    return key_char;
}

static int usb_read(uint8_t *buffer, size_t size)
{
    if (size < USB_KEYBOARD_BUFFER_SIZE)
        return -1;

    for (size_t i = 0; i < size; ++i) {
        buffer[i] = 0;
    }

    uint8_t scancode;
    asm volatile(
        "inb $0x60, %%al\n"
        : "=a"(scancode)
        :
        : );

    buffer[2] = scancode;

    if (scancode & 0x80) {
        buffer[2] = 0;
        return USB_KEYBOARD_BUFFER_SIZE;
    }

    static const char scancode_to_ascii[] = {
        0, 0x1B, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
        '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0,
        ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0',
        '.', 0, 0, 0, 0
    };

    char translated_char = 0;
    if (scancode < sizeof(scancode_to_ascii) && scancode != BACKSPACE_SCANCODE)
    {
        translated_char = scancode_to_ascii[scancode];
    }
    buffer[0] = translated_char;
    buffer[1] = 0;

    return USB_KEYBOARD_BUFFER_SIZE;
}