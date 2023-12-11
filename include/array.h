/*
 * Simple wrapper around raw arrays
 */

#ifndef OS_PROXY_ARRAY_TYPE_H
#define OS_PROXY_ARRAY_TYPE_H 1

typedef char data_t;

#include <stddef.h>
#include <stdint.h>

typedef struct array
{
    data_t* data;
    size_t len;
    size_t capacity;
} array_t;

array_t* array_init(size_t len);

// !!!! realloc every time
int array_extend(array_t *array, const data_t* const data, const size_t n_data);

#endif /* OS_PROXY_ARRAY_TYPE_H */