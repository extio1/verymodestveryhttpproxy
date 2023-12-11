#include "../include/aux.h"

#include <stdio.h>

void 
print_array_uint8(const uint8_t* const arr, const int len)
{
    for(int i = 0; i < len; ++i){
        printf("%d", arr[i]);
    }
    printf("\n");
}

void 
print_array_char(const unsigned char* const arr, const int len)
{
    for(int i = 0; i < len; ++i){
        printf("%c", arr[i]);
    }
    printf("\n");
}
