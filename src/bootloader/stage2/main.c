#include "stdint.h"
#include "stdio.h"

void _cdecl cstart_(uint16_t bootDrive){
    int n = 5;
    char c = 'A';
    char* s = "Hello, World!";
    printf("This is a printf test! %d %c %s %o", n, c, s, n);
}
