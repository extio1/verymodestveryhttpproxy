#include "../include/map.h"

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

struct list_entry* 
list_entry_init(header_key_t key, header_value_t value)
{
    struct list_entry* en = malloc(sizeof(struct list_entry));
    en->key = key;
    en->value = value;
    
    return en;
}

list_t* 
list_init()
{
    list_t* list = malloc(sizeof(list_t));
    list->first_entry = NULL;
    return list;
}

void
list_append(list_t* list, header_key_t key, header_value_t value)
{
    struct list_entry* entry = list->first_entry; 
    if(list->first_entry == NULL){
        list->first_entry = list_entry_init(key, value);
        return;
    }
    struct list_entry* next_entry = entry->next_entry;

    while(next_entry != NULL){
        entry = next_entry;
        next_entry = next_entry->next_entry;
    }

    entry->next_entry = list_entry_init(key, value);
    entry->next_entry->next_entry = NULL;
}

header_value_t 
list_get(const list_t* const list, header_key_t key)
{
    node_t* node = list->first_entry;
    if(node == NULL)
        return NULL;
    node_t* nnode = node->next_entry;

    if(strcmp(node->key, key) == 0){
        return node->value;
    }

    while(nnode != NULL){
        if(strcmp(nnode->key, key) == 0){
            return nnode->value;
        }
        nnode = nnode->next_entry;
    }

    return NULL;
}

void 
list_print(const list_t* const list)
{
    node_t* node = list->first_entry;
    if(node == NULL)
        return;
    node_t* nnode = node->next_entry;

    printf("%s: %s\n", node->key, node->value);

    while(nnode != NULL){
        printf("%s: %s\n", nnode->key, nnode->value);
        nnode = nnode->next_entry;
    }
}