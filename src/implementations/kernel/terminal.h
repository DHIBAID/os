#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../../interface/printf.h"
#include "../../interface/string.h"
#include "../../interface/util.h"
#include "../x86_64/drivers/disk.h"
#include "kernel.h"
#include "memory.h"

void update_input(char c);
void parse_command(char* command);

extern char* currentDirectory;
extern FAT32_Info global_fat32;
extern uint32_t current_dir_cluster;
extern int fat32_initialized;
