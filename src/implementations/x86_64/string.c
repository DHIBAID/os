#include "string.h"

int strcmp(const char *str1, const char *str2){
    while (*str1 && (*str1 == *str2)){
        str1++;
        str2++;
    }
    
    return *(unsigned char *)str1 - *(unsigned char *)str2;
}

int strncmp(const char *str1, const char *str2, size_t n){
    while (n && *str1 && (*str1 == *str2)){
        str1++;
        str2++;
        n--;
    }
    
    if (n == 0){
        return 0;
    }
    
    return *(unsigned char *)str1 - *(unsigned char *)str2;
}