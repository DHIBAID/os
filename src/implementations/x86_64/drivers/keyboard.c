#include "keyboard.h"
#include "display.h" // Include display driver header for remove_char_at_cursor

// Define the backspace scancode
#define BACKSPACE_SCANCODE 0x0E

static int usb_read(uint8_t *buffer, size_t size);

char bios_get_char()
{
    char c = 0;
    // Loop until a character (non-zero) is read
    while ((c = read_from_usb_keyboard()) == 0)
    {
        // Add a small delay or yield if in a multitasking environment
        // to prevent busy-waiting consuming too much CPU.
        // For simplicity in a basic OS, this loop might be okay.
        asm volatile("pause"); // Example delay/yield hint
    }
    return c;
}

// This function is no longer needed as we check the scancode directly
/*
int is_backspace(char c)
{
    return c == '\b'; // Backspace key ASCII value
}
*/

char read_from_usb_keyboard()
{
    static uint8_t last_scancode = 0; // Store the last scancode read
    static uint8_t key_released = 1;  // Track if the key was released
    uint8_t buffer[USB_KEYBOARD_BUFFER_SIZE];
    char key_char = 0; // The character to return

    // usb_read now fills the buffer and translates the scancode if it's printable
    if (usb_read(buffer, USB_KEYBOARD_BUFFER_SIZE) > 0)
    {
        uint8_t current_scancode = buffer[2]; // The raw scancode read from the port

        // Check for key press event (scancode is not 0)
        if (current_scancode != 0)
        {
            // Check if it's a new key press or a repeat after release
            // This basic check prevents auto-repeat from flooding input
            if (current_scancode != last_scancode || key_released)
            {
                // Handle backspace directly using its scancode
                if (current_scancode == BACKSPACE_SCANCODE)
                {
                    remove_char_at_cursor();
                    key_char = 0; // Backspace action performed, don't return a character
                }
                else
                {
                    // Translate other scancodes to ASCII
                    // This translation logic is now moved inside usb_read
                    // usb_read puts the translated char (or 0) in buffer[2]
                    key_char = (char)buffer[0]; // Use the translated char from usb_read
                }

                last_scancode = current_scancode; // Update the last scancode
                key_released = 0;                 // Mark key as pressed
            }
            // else: Key is being held down, ignore repeat for now
        }
        else // Scancode is 0, indicating key release or no key pressed
        {
            key_released = 1;    // Mark key as released
            last_scancode = 0; // Reset last scancode
        }
    }

    return key_char; // Return the translated character or 0
}

static int usb_read(uint8_t *buffer, size_t size)
{
    if (size < USB_KEYBOARD_BUFFER_SIZE)
        return -1;

    // Clear buffer initially
    for (size_t i = 0; i < size; ++i) {
        buffer[i] = 0;
    }

    uint8_t scancode;
    // Read scancode from keyboard port
    asm volatile(
        "inb $0x60, %%al\n" // Read scancode from port 0x60
        : "=a"(scancode)
        :
        : );

    // Store the raw scancode for the caller (read_from_usb_keyboard) to check
    buffer[2] = scancode;

    // --- Scancode Translation (only if not backspace or other special handling) ---
    // Check if it's a key release code (most significant bit set)
    if (scancode & 0x80) {
        // It's a key release, clear the scancode field in buffer
        // The main logic in read_from_usb_keyboard handles release state
         buffer[2] = 0; // Indicate no active key press
         return USB_KEYBOARD_BUFFER_SIZE; // Return successfully
    }


    // --- Handle Backspace ---
    // Backspace is handled in the caller (read_from_usb_keyboard) now
    // We just pass the raw scancode up.

    // --- Translate other scancodes to ASCII ---
    static const char scancode_to_ascii[] = {
        // Scancodes 0x00 to 0x58 (Set 1)
        0,    0x1B, '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0',  '-',  '=',  '\b', // 0x00 - 0x0E
        '\t', 'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',  'o',  'p',  '[',  ']',  '\n', // 0x0F - 0x1C (Enter)
        0,    'a',  's',  'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',  '\'', '`',  0,    // 0x1D (LCtrl) - 0x2A (LShift)
        '\\', 'z',  'x',  'c',  'v',  'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',  0,    // 0x2B - 0x37 (RShift, KP_*) - 0x38 (LAlt)
        ' ',  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    // 0x39 (Space) - 0x44 (CapsLock ... F10)
        0,    0,    '7',  '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',  '2',  '3',  '0',  // 0x45 (NumLock) - 0x52 (KP_0)
        '.', 0, 0, 0, 0 // 0x53 (KP_.) ...
        // Add more mappings if needed, up to index 0x58
    };

    // We only handle printable characters here.
    // Modifiers (Shift, Ctrl, Alt) and special keys (like Delete, Arrows)
    // are not translated to a character here. Backspace is handled above.
    char translated_char = 0;
    if (scancode < sizeof(scancode_to_ascii) && scancode != BACKSPACE_SCANCODE)
    {
        translated_char = scancode_to_ascii[scancode];
    }
    // Store the translated char (or 0 if not printable/mapped) in buffer[0]
    // buffer[2] still holds the raw scancode.
    buffer[0] = translated_char;


    // Modifier keys (like Shift, Ctrl, Alt) are typically read from buffer[0]
    // in a standard USB HID report, but here we are reading raw scancodes.
    // Handling modifiers would require tracking their press/release state based on scancodes.
    // For simplicity, modifier handling is omitted here.
    buffer[1] = 0; // Reserved byte

    // Fill the rest of the buffer (if any) - already done at the start

    return USB_KEYBOARD_BUFFER_SIZE; // Return the number of bytes "read" (filled)
}