#include "kernel/terminal.h"

char* inp;
int len = 0;

void update_input(char c) {
    if (c == '\n') {
        print_newline();
        parse_command(inp);
        // Reset input buffer after processing command
        len = 0;
        *(inp + len) = '\0';
        print_str(strconcat(currentDirectory, ">: "));
        return;
    } else if (c == '\b') {
        if (len > 0) {
            len--;
            *(inp + len) = '\0';
        }
        return;
    }

    if (len < 255) {
        *(inp + len) = c;
        len++;
        *(inp + len) = '\0';
    }  // solf lock to prevent buffer overflow
}

void parse_command(char* command) {
    if (strcmp(command, "clear") == 0) {
        print_clear();
    } else if (strcmp(command, "help") == 0) {
        print_str("Available commands:\n");
        print_str("clear - Clear the screen\n");
        print_str("help - Show this help message\n");
        print_str("reboot - Reboot the system\n");
        print_str("shutdown - Shutdown the system\n");
        print_str("echo <message> - Print a message\n");
        print_str("meminfo - Show memory information\n");
        print_str("ls - List files in the current directory\n");
        print_str("cd <directory> - Change the current directory\n");
        print_str("cat <file> - Display file contents\n");
        print_str("touch <file> - Create a new file\n");
        print_str("mkdir <directory> - Create a new directory\n");
        print_str("rmdir <directory> - Remove a directory\n");
        print_str("rm <file> - Delete a file\n");
    } else if (strcmp(command, "reboot") == 0) {
        print_str("Rebooting...\n");
        sleep(1000);
        asm volatile("int $0x19");  // BIOS interrupt for reboot
    } else if (strcmp(command, "shutdown") == 0) {
        print_str("Shutting down...\n");
        sleep(2000);

        outw(0x2000, 0x604);

        // fallback
        print_str("System halted, safe to hard reset.\n");
        asm volatile("hlt");
    } else if (strncmp(command, "echo", 4) == 0) {
        char* message = command + 5;
        print_str(message);
        print_str("\n");
    } else if (strcmp(command, "meminfo") == 0) {
        meminfo();
    } else if (strcmp(command, "ls") == 0) {
        current_dir_cluster = global_fat32.root_cluster;
        list_dir(&global_fat32, current_dir_cluster);
    } else if (strncmp(command, "cd ", 3) == 0) {
        change_directory(command + 3);
    } else if (strcmp(command, "cd") == 0) {
        // cd without arguments - go to root
        change_directory("/");
    } else if (strncmp(command, "cat ", 4) == 0) {
        char* arg = command + 4;

        current_dir_cluster = global_fat32.root_cluster;
        fat32_cat(&global_fat32, arg);
    } else if (strncmp(command, "touch ", 6) == 0) {
        char* arg = command + 6;
        if (fat32_create_file(&global_fat32, current_dir_cluster, arg) == 0) {
            print_str("File created\n");
        } else {
            print_str("Failed to create file\n");
        }
    } else if (strncmp(command, "mkdir ", 6) == 0) {
        char* arg = command + 6;
        if (fat32_create_directory(&global_fat32, current_dir_cluster, arg) == 0) {
            print_str("Directory created\n");
        } else {
            print_str("Failed to create directory\n");
        }
    } else if (strncmp(command, "rmdir ", 6) == 0) {
        char* arg = command + 6;
        if (fat32_remove_directory(&global_fat32, current_dir_cluster, arg) == 0) {
            print_str("Directory removed\n");
        } else {
            print_str("Failed to remove directory\n");
        }
    } else if (strncmp(command, "rm ", 3) == 0) {
        char* arg = command + 3;
        if (fat32_delete_file(&global_fat32, current_dir_cluster, arg) == 0) {
            print_str("Deleted\n");
        } else {
            print_str("Failed to delete\n");
        }
    } else if (strcmp(command, "") == 0) {
        // Do nothing for empty command
    } else {
        print_str("Unknown command: ");
        print_str(command);
        print_str("\n");
    }
}
