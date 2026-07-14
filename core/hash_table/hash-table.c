#include "hash-table.h"

static unsigned int ht_hash(const void* key, size_t ks) {
    uint32_t hash = 0x7F4A7C15;
    for (size_t i = 0; i < ks; i++) {
        uint32_t c = *((unsigned char*) key++);
        hash ^= (c + (hash << 5) + (hash >> 3));
        hash = (hash << 11) | (hash >> 21);
        hash += (c * 97);
        hash ^= (hash >> 13);
    }

    hash ^= (hash << 7);
    hash += (hash >> 17);
    hash ^= (hash << 5);

    return hash % TABLE_SIZE;
}
HashTable* ht_create(void) {
    return calloc(1, sizeof(HashTable));
}

void ht_set(HashTable* ht, const char* key, void* value) {
    size_t ks = strlen(key) + 1;
    unsigned int idx = ht_hash(key, ks);
    for (Entry* e = ht->buckets[idx]; e; e = e->next)
        if (e->key_size == ks && memcmp(e->key, key, ks) == 0) {
            free(e->value);
            e->value = value;
            return;
        }
    Entry* ne = malloc(sizeof(Entry));
    ne->key = malloc(ks);
    memcpy(ne->key, key, ks);
    ne->key_size = ks;
    ne->value = value;
    ne->next = ht->buckets[idx];
    ht->buckets[idx] = ne;
    ht->count++;
}
void* ht_get(HashTable* ht, const char* key) {
    size_t ks = strlen(key) + 1;
    unsigned int idx = ht_hash(key, ks);
    for (Entry* e = ht->buckets[idx]; e; e = e->next)
        if (e->key_size == ks && memcmp(e->key, key, ks) == 0)
            return e->value;
    return NULL;
}
int ht_delete(HashTable* ht, const char* key) {
    size_t ks = strlen(key) + 1;
    unsigned int idx = ht_hash(key, ks);
    for (Entry** ep = &ht->buckets[idx]; *ep; ep = &(*ep)->next) {
        Entry* e = *ep;
        if (e->key_size == ks && memcmp(e->key, key, ks) == 0) {
            *ep = e->next;
            free(e->key);
            free(e->value);
            free(e);
            ht->count--;
            return 1;
        }
    }
    return 0;
}
void ht_destroy(HashTable* ht) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        Entry* e = ht->buckets[i];
        while (e) {
            Entry* n = e->next;
            free(e->key);
            free(e->value);
            free(e);
            e = n;
        }
    }
    free(ht);
}
