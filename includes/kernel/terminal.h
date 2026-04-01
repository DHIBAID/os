#pragma once

#ifndef KERNEL_TERMINAL_H
#define KERNEL_TERMINAL_H

#include <stddef.h>
#include <stdint.h>

#include "drivers/disk.h"
#include "drivers/memory.h"
#include "kernel/kernel.h"
#include "lib/printf.h"
#include "lib/string.h"
#include "lib/util.h"

void update_input(char c);
void parse_command(char* command);

extern char currentDirectory[256];
extern FAT32_Info global_fat32;
extern uint32_t current_dir_cluster;
extern int fat32_initialized;

#endif  // KERNEL_TERMINAL_H