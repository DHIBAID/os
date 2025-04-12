#include "../../interface/util.h"

void sleep(int ms)
{
    for (int i = 0; i < ms * 1000; i++)
    {
        asm volatile("pause");
    }
}