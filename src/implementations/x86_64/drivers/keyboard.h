#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include "./display.h"

#define USB_KEYBOARD_BUFFER_SIZE 16

char bios_get_char();
char read_from_usb_keyboard();
int is_backspace(char c);