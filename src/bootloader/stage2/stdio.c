#include "stdio.h"
#include "x86.h"

void putc(char c){
    x86_Video_WriteCharTeletype(c, 0);
}
void puts(const char* str){
    while(*str){
        putc(*str);
        str++;
    }
}

#define PRINTF_NORMAL_STATE 0
#define PRINTF_LENGTH_STATE 1
#define PRINTF_STATE_LENGTH_SHORT 2
#define PRINTF_STATE_LENGTH_LONG 3
#define PRINTF_STATE_SPEC 4

#define PRINTF_LENGTH_DEFAULT 0
#define PRINTF_LENGTH_SHORT_SHORT 1
#define PRINTF_LENGTH_SHORT 2
#define PRINTF_LENGTH_LONG 3
#define PRINTF_LENGTH_LONG_LONG 4

int* printf_number(int* argp, int length, bool sign, int radix);

void _cdecl printf(const char* fmt, ...){
    int state = PRINTF_NORMAL_STATE;
    int* argp = (int *) &fmt;
    int length = PRINTF_LENGTH_DEFAULT;
    int radix = 10;
    int sign = false;

    argp += sizeof(fmt) / sizeof(int);

    while(*fmt){

        switch (state){
            case PRINTF_NORMAL_STATE:
                switch (*fmt){
                case '%':
                    state = PRINTF_LENGTH_STATE;
                    break;
                
                default:
                    putc(*fmt);
                    break;
                }
                break;
            
            case PRINTF_LENGTH_STATE:
            switch (*fmt){
                case 'h':
                    length = PRINTF_LENGTH_SHORT;
                    state = PRINTF_STATE_LENGTH_SHORT;
                    break;

                case 'l':
                    length = PRINTF_LENGTH_LONG;
                    state = PRINTF_STATE_LENGTH_LONG;
                    break;

                default:
                    goto PRINTF_STATE_SPEC_;
            }
            break;

            case PRINTF_STATE_LENGTH_SHORT:
                if (*fmt == 'h'){
                    length = PRINTF_LENGTH_SHORT_SHORT;
                    state = PRINTF_STATE_SPEC;
                } else {
                    goto PRINTF_STATE_SPEC_;
                }
                break;

            case PRINTF_STATE_LENGTH_LONG:
                if (*fmt == 'l'){
                    length = PRINTF_LENGTH_LONG_LONG;
                    state = PRINTF_STATE_SPEC;
                } else {
                    goto PRINTF_STATE_SPEC_;
                }
                break;

            case PRINTF_STATE_SPEC:
            PRINTF_STATE_SPEC_:
                switch(*fmt){
                    case 'c': putc((char) *argp); argp++; break;
                    case 's': puts(*(char**) argp); argp++; break;

                    case '%': putc('%'); break;

                    case 'd':
                    case 'i':
                        radix = 10; sign = true;
                        argp = printf_number(argp, length, radix, sign);
                        break;

                    case 'u':
                        radix = 10; sign = false;
                        argp = printf_number(argp, length, radix, sign);
                        break;

                    case 'x':
                    case 'X':
                    case 'p':
                        radix = 16; sign = false;
                        argp = printf_number(argp, length, radix, sign);
                        break;

                    case 'o':
                        radix = 8; sign = false;
                        argp = printf_number(argp, length, radix, sign);
                        break;

                    default: break; //ignore any other specifier used.

                }
            
            // reset state to NORMAL and reset all the variables
            state = PRINTF_NORMAL_STATE;
            length = PRINTF_LENGTH_DEFAULT;
            radix = 10;
            sign = false;
            break;
        }
        
        fmt++;
    }

}

const char g_HexChars[16] = "0123456789abcdef";

int* printf_number(int* argp, int length, bool sign, int radix){
    char buffer[32];
    unsigned long long number;
    int number_sign = 1;
    int pos = 0;

    // printf based on length
    switch (length){
        case PRINTF_LENGTH_SHORT_SHORT:
        case PRINTF_STATE_LENGTH_SHORT:
        case PRINTF_LENGTH_DEFAULT:
            if (sign) {
                int n = *argp;
                if (n < 0){
                    number_sign = -1;
                    n = -n;
                }
                number = n;
            } else {
                number = *(unsigned long long*) argp;
            }

            argp++;
            break;

        case PRINTF_LENGTH_LONG:
            if (sign) {
                long int n = *(long int *) argp;
                if (n < 0){
                    number_sign = -1;
                    n = -n;
                }
                number = n;
            } else {
                number = *(unsigned long long*) argp;
            }

            argp += 2;
            break;

        case PRINTF_LENGTH_LONG_LONG:
            if (sign) {
                long long int n = *(long long int *) argp;
                if (n < 0){
                    number_sign = -1;
                    n = -n;
                }
                number = n;
            } else {
                number = *(unsigned long long *) argp;
            }

            argp += 4;
            break;
        
    }

    // convert number to string
    do {

        uint32_t rem; 
        x86_div64_32(number, radix, &number, &rem);
        buffer[pos++] = g_HexChars[rem];

    } while (number > 0);

    // add sign
    if (sign && number_sign < 0){
        buffer[pos++] = '-';
    }

    // print fin reverse order
    while (--pos >= 0){
        putc(buffer[pos]);
    }

    return argp;
    
}
