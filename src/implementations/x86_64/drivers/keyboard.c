#include "keyboard.h"

static int usb_read(uint8_t *buffer, size_t size);

char bios_get_char()
{
    char c = 0;
    while ((c = read_from_usb_keyboard()) == 0);
    return c;
}

char read_from_usb_keyboard()
{
    static char last_key = 0; // Store the last key pressed
    static uint8_t key_released = 1; // Track if the key was released
    uint8_t buffer[USB_KEYBOARD_BUFFER_SIZE];
    char key = 0;

    if (usb_read(buffer, USB_KEYBOARD_BUFFER_SIZE) > 0)
    {
        if (buffer[2] != 0) 
        {
            if (buffer[2] != last_key || key_released) 
            {
                key = buffer[2];
                last_key = key; // Update the last key pressed
                key_released = 0; // Mark key as pressed
            }
            else 
            {
                key = 0; // Prevent duplicate key presses
            }
        }
        else 
        {
            key_released = 1; // Mark key as released
            last_key = 0; // Reset last key when no key is pressed
        }
    }

    return key;
}

static int usb_read(uint8_t *buffer, size_t size)
{
    if (size < USB_KEYBOARD_BUFFER_SIZE)
        return -1;

    buffer[0] = 0; // Modifier byte
    buffer[1] = 0; // Reserved byte

    // Read from keyboard port
    asm volatile (
        "mov $0x60, %%dx\n"
        "inb %%dx, %%al\n"
        : "=a"(buffer[2])
        :
        : "%dx"
    );

    static const char scancode_to_ascii[] = {
        0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
        '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
        '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    static const char special_keys[] = {
        [0x1D] = 0x11, // Control
        [0x2A] = 0x10, // Left Shift
        [0x36] = 0x10, // Right Shift
        [0x38] = 0x12, // Alt
        [0xE0] = 0x1B, // Escape
        [0x53] = 0x7F  // Delete
    };

    if (buffer[2] < sizeof(scancode_to_ascii)) {
        buffer[2] = scancode_to_ascii[buffer[2]];
    } else if (buffer[2] < sizeof(special_keys) && special_keys[buffer[2]] != 0) {
        buffer[2] = special_keys[buffer[2]];
    } else {
        buffer[2] = 0; // Invalid scancode
    }

    for (size_t i = 3; i < USB_KEYBOARD_BUFFER_SIZE; i++)
    {
        buffer[i] = 0;
    }

    return USB_KEYBOARD_BUFFER_SIZE; // Return the number of bytes read
}