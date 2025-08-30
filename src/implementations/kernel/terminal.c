// #include "terminal.h"
#include "../../interface/printf.h"
#include "../../interface/string.h"

char *inp;
int len = 0;

void update_input(char c)
{
    if (c == '\n')
    {
        print_newline();
        parse_command(inp);
        inp[len] = '\0';
        len = 0;
        print_str("/>: ");
        return;
    }
    else if (c == '\b')
    {
        if (len > 0)
        {
            len--;
            inp[len] = '\0';
        }
        return;
    }

    if (len < 255)
    {
        inp[len] = c;
        len++;
        inp[len] = '\0';
    } // solf lock to prevent buffer overflow
}

void parse_command(char *command)
{
    if (strcmp(command, "clear") == 0)
    {
        print_clear();
    }
    else if (strcmp(command, "help") == 0)
    {
        print_str("Available commands:\n");
        print_str("clear - Clear the screen\n");
        print_str("help - Show this help message\n");
        print_str("reboot - Reboot the system\n");
        print_str("shutdown - Shutdown the system\n");
        print_str("echo <message> - Print a message\n");
        print_str("meminfo - Show memory information\n");
    }
    else if (strcmp(command, "reboot") == 0)
    {
        print_str("Rebooting...\n");
        sleep(1000);
        asm volatile("int $0x19"); // BIOS interrupt for reboot
    }
    else if (strcmp(command, "shutdown") == 0)
    {
        print_str("Shutting down...\n");
        sleep(2000);

        outw(0x2000, 0x604);

        // fallback
        print_str("System halted, safe to hard reset.\n");
        asm volatile("hlt");
    }
    else if (strncmp(command, "echo", 4) == 0)
    {
        char *message = command + 5;
        print_str(message);
        print_str("\n");
    }
    else if (strcmp(command, "meminfo") == 0)
    {
        meminfo();
    }
    else
    {
        print_str("Unknown command: ");
        print_str(command);
        print_str("\n");
    }
}
