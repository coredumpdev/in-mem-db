#include "eval.h"

static const char* arg(ASTNode* node, int n)
{
    ArgNode* a = node->args;
    for (int i = 0; i < n && a; i++)
        a = a->next;
    return a ? a->value : NULL;
}

int eval(ASTNode* node, HashTable* db)
{
    switch (node->kind)
    {
    case CMD_SET:
        ht_set(db, arg(node, 0), strdup(arg(node, 1)));
        puts("OK");
        break;
    case CMD_GET:
    {
        char* v = ht_get(db, arg(node, 0));
        v ? printf("\"%s\"\n", v) : puts("(nil)");
        break;
    }
    case CMD_DEL:
    {
        int n = 0;
        for (ArgNode* a = node->args; a; a = a->next)
            n += ht_delete(db, a->value);
        printf("(integer) %d\n", n);
        break;
    }
    case CMD_EXISTS:
        printf("(integer) %d\n", ht_get(db, arg(node, 0)) ? 1 : 0);
        break;
    case CMD_APPEND:
    {
        const char *key = arg(node, 0), *suf = arg(node, 1);
        char* ex = ht_get(db, key);
        char* nv;
        if (ex)
        {
            size_t len = strlen(ex) + strlen(suf) + 1;
            nv = malloc(len);
            snprintf(nv, len, "%s%s", ex, suf);
        }
        else
        {
            nv = strdup(suf);
        }
        ht_set(db, key, nv);
        printf("(integer) %zu\n", strlen(nv));
        break;
    }
    case CMD_RENAME:
    {
        char* v = ht_get(db, arg(node, 0));
        if (!v)
        {
            puts("ERR no such key");
            break;
        }
        ht_set(db, arg(node, 1), strdup(v));
        ht_delete(db, arg(node, 0));
        puts("OK");
        break;
    }
    case CMD_MSET:
    {
        ArgNode* a = node->args;
        while (a && a->next)
        {
            ht_set(db, a->value, strdup(a->next->value));
            a = a->next->next;
        }
        puts("OK");
        break;
    }
    case CMD_MGET:
    {
        int i = 1;
        for (ArgNode* a = node->args; a; a = a->next, i++)
        {
            char* v = ht_get(db, a->value);
            v ? printf("%d) \"%s\"\n", i, v) : printf("%d) (nil)\n", i);
        }
        break;
    }
    case CMD_KEYS:
    {
        int n = 0;
        for (int i = 0; i < TABLE_SIZE; i++)
            for (Entry* e = db->buckets[i]; e; e = e->next)
                printf("%d) \"%s\"\n", ++n, (char*) e->key);
        if (!n)
            puts("(empty)");
        break;
    }
    case CMD_DBSIZE:
        printf("(integer) %d\n", db->count);
        break;
    case CMD_FLUSHDB:
    {
        for (int i = 0; i < TABLE_SIZE; i++)
        {
            Entry* e = db->buckets[i];
            while (e)
            {
                Entry* n = e->next;
                free(e->key);
                free(e->value);
                free(e);
                e = n;
            }
            db->buckets[i] = NULL;
        }
        db->count = 0;
        puts("OK");
        break;
    }
    case CMD_HELP:
        puts("  SET    <key> <value>\n"
             "  GET    <key>\n"
             "  DEL    <key> [key ...]\n"
             "  EXISTS <key>\n"
             "  APPEND <key> <value>\n"
             "  RENAME <old> <new>\n"
             "  MSET   <k> <v> [k v ...]\n"
             "  MGET   <key> [key ...]\n"
             "  KEYS / DBSIZE / FLUSHDB\n"
             "  EXIT / QUIT");
        break;
    case CMD_EXIT:
        return 1;
    default:
        break;
    }
    return 0;
}
