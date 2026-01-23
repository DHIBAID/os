#pragma once

#include <stddef.h>
#include <stdint.h>

#include "drivers/memory.h"

int strcmp(const char* str1, const char* str2);
int strncmp(const char* str1, const char* str2, size_t n);
char* strconcat(const char* str1, const char* str2);
size_t strlen(const char* str);
char* strcpy(char* dest, const char* src);
char* strcat(char* dest, const char* src);