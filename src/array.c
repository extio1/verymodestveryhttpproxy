#include "../include/array.h"

#include <stdlib.h>
#include <stdio.h>

array_t*
array_init(size_t len)
{
    array_t* arr = malloc(sizeof(array_t));
    arr->data = malloc(sizeof(data_t)*len);
    arr->capacity = len;
    arr->len = 0;
    return arr;
}

int 
array_extend(array_t* array, const data_t* const data, const size_t n_data)
{
    data_t* new_data = realloc(array->data, array->len+n_data);
    if(new_data == NULL){
        perror("realloc() error");
        return -1;
    }
    array->data = new_data;

    for(int i = 0; i < n_data; ++i){
        array->data[array->len+i] = data[i];
    }

    array->len += n_data;

    return 0;
}
