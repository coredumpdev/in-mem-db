#include "hash-table.h"


static unsigned int ht_hash(const void *key, size_t ks) {
    const unsigned char *p = key; unsigned int h = 5381;
    for (size_t i = 0; i < ks; i++) h = ((h << 5) + h) + p[i];
    return h % TABLE_SIZE;
}
HashTable *ht_create(void) { return calloc(1, sizeof(HashTable)); }
 
void ht_set(HashTable *ht, const char *key, void *value) {
    size_t ks = strlen(key)+1;
    unsigned int idx = ht_hash(key, ks);
    for (Entry *e = ht->buckets[idx]; e; e = e->next)
        if (e->key_size == ks && memcmp(e->key, key, ks) == 0)
            { free(e->value); e->value = value; return; }
    Entry *ne = malloc(sizeof(Entry));
    ne->key = malloc(ks); memcpy(ne->key, key, ks);
    ne->key_size = ks; ne->value = value;
    ne->next = ht->buckets[idx]; ht->buckets[idx] = ne; ht->count++;
}
void *ht_get(HashTable *ht, const char *key) {
    size_t ks = strlen(key)+1; unsigned int idx = ht_hash(key, ks);
    for (Entry *e = ht->buckets[idx]; e; e = e->next)
        if (e->key_size == ks && memcmp(e->key, key, ks) == 0) return e->value;
    return NULL;
}
int ht_delete(HashTable *ht, const char *key) {
    size_t ks = strlen(key)+1; unsigned int idx = ht_hash(key, ks);
    for (Entry **ep = &ht->buckets[idx]; *ep; ep = &(*ep)->next) {
        Entry *e = *ep;
        if (e->key_size == ks && memcmp(e->key, key, ks) == 0) {
            *ep = e->next; free(e->key); free(e->value); free(e); ht->count--; return 1;
        }
    }
    return 0;
}
void ht_destroy(HashTable *ht) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        Entry *e = ht->buckets[i];
        while (e) { Entry *n=e->next; free(e->key); free(e->value); free(e); e=n; }
    }
    free(ht);
}
 


