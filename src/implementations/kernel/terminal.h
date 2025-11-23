#pragma once

#include <stdint.h>
#include <stddef.h>

#include "../../interface/util.h"
#include "../x86_64/drivers/disk.h"

void update_input(char c);
void parse_command(char *command);