#include "stdint.h"
#include "stdio.h"

void _cdecl cstart_(uint16_t bootDrive){
    int n = 5;
    printf("This is a printf test! %d", n);
}
