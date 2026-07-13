#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include "../../utils/log.h"
#include "../../globals/enums.h"
#include <stdlib.h>
#include <string.h>


#define TABLE_SIZE 64


typedef struct Entry {
    void         *key;
    void         *value;
    size_t        key_size;
    struct Entry *next;
} Entry;


typedef struct {
    Entry *buckets[TABLE_SIZE];
    int    count;
} HashTable;


HashTable *ht_create(void);
void       ht_destroy(HashTable *ht);
void       ht_set(HashTable *ht, const char *key, void *value);
void      *ht_get(HashTable *ht, const char *key);
int        ht_delete(HashTable *ht, const char *key);


#define HT_SET_STR(ht, k, v)        ht_set((ht), (k), (v))
#define HT_GET_STR(ht, k)           ht_get((ht), (k))
#define HT_DEL_STR(ht, k)           ht_delete((ht), (k))

#endif
