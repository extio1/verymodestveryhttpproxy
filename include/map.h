/*
 * Simple key-value storage, where key and value are const char* 
 */

#ifndef OS_PROXY_MESSAGE_H
#define OS_PROXY_MESSAGE_H 1

#include <stddef.h>
#include <stdint.h>

typedef char* header_key_t;
typedef char* header_value_t;

typedef struct list_entry
{
    header_key_t key;
    header_value_t value;

    struct list_entry* next_entry;
} node_t;

typedef struct list
{
    struct list_entry* first_entry;
} list_t;

struct list_entry* list_entry_init(header_key_t key, header_value_t value);

list_t* list_init();
void list_append(list_t* list, header_key_t key, header_value_t value);
void list_print(const list_t* const list);
header_value_t list_get(const list_t* const list, header_key_t key);


#endif /* OS_PROXY_MESSAGE_H */