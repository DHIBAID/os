#include "lib/string.h"

int strcmp(const char* str1, const char* str2) {
    while (*str1 && (*str1 == *str2 && *str2 && *str1)) {
        str1++;
        str2++;
    }

    return *(unsigned char*)str1 - *(unsigned char*)str2;
}

int strncmp(const char* str1, const char* str2, size_t n) {
    while (n && *str1 && (*str1 == *str2)) {
        str1++;
        str2++;
        n--;
    }

    if (n == 0) {
        return 0;
    }

    return *(unsigned char*)str1 - *(unsigned char*)str2;
}

char* strconcat(const char* str1, const char* str2) {
    size_t len1 = 0;
    size_t len2 = 0;

    while (str1[len1] != '\0') len1++;
    while (str2[len2] != '\0') len2++;

    char* result = (char*)kmalloc(len1 + len2 + 1);
    for (size_t i = 0; i < len1; i++) {
        result[i] = str1[i];
    }
    for (size_t j = 0; j < len2; j++) {
        result[len1 + j] = str2[j];
    }
    result[len1 + len2] = '\0';

    return result;
}

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

char* strcpy(char* dest, const char* src) {
    char* orig_dest = dest;
    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';
    return orig_dest;
}

char* strcat(char* dest, const char* src) {
    char* orig_dest = dest;
    // Find end of dest
    while (*dest != '\0') {
        dest++;
    }
    // Copy src to end of dest
    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';
    return orig_dest;
}

char* u64_to_hex(uint64_t value, char* buf) {
    // buf must be at least 19 bytes ("0x" + 16 hex digits + '\0')
    const char* digits = "0123456789ABCDEF";
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 16; i++)
        buf[2 + i] = digits[(value >> (60 - i * 4)) & 0xF];
    buf[18] = '\0';
    return buf;
}