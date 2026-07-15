#include "eval.h"
#include "../core/outbuf.h"

static const char* arg(ASTNode* node, int n)
{
    ArgNode* a = node->args;
    for (int i = 0; i < n && a; i++)
        a = a->next;
    return a ? a->value : NULL;
}

int eval(ASTNode* node, HashTable* db, char* out, size_t cap, size_t* out_len)
{
    switch (node->kind)
    {
    case CMD_SET:
        ht_set(db, arg(node, 0), strdup(arg(node, 1)));
        out_write(out, cap, out_len, "OK\n");
        break;
    case CMD_GET:
    {
        char* v = ht_get(db, arg(node, 0));
        v ? out_write(out, cap, out_len, "\"%s\"\n", v) : out_write(out, cap, out_len, "(nil)\n");
        break;
    }
    case CMD_DEL:
    {
        int n = 0;
        for (ArgNode* a = node->args; a; a = a->next)
            n += ht_delete(db, a->value);
        out_write(out, cap, out_len, "(integer) %d\n", n);
        break;
    }
    case CMD_EXISTS:
        out_write(out, cap, out_len, "(integer) %d\n", ht_get(db, arg(node, 0)) ? 1 : 0);
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
        out_write(out, cap, out_len, "(integer) %zu\n", strlen(nv));
        break;
    }
    case CMD_RENAME:
    {
        char* v = ht_get(db, arg(node, 0));
        if (!v)
        {
            out_write(out, cap, out_len, "ERR no such key\n");
            break;
        }
        ht_set(db, arg(node, 1), strdup(v));
        ht_delete(db, arg(node, 0));
        out_write(out, cap, out_len, "OK\n");
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
        out_write(out, cap, out_len, "OK\n");
        break;
    }
    case CMD_MGET:
    {
        int i = 1;
        for (ArgNode* a = node->args; a; a = a->next, i++)
        {
            char* v = ht_get(db, a->value);
            v ? out_write(out, cap, out_len, "%d) \"%s\"\n", i, v)
              : out_write(out, cap, out_len, "%d) (nil)\n", i);
        }
        break;
    }
    case CMD_KEYS:
    {
        int n = 0;
        for (int i = 0; i < TABLE_SIZE; i++)
            for (Entry* e = db->buckets[i]; e; e = e->next)
                out_write(out, cap, out_len, "%d) \"%s\"\n", ++n, (char*) e->key);
        if (!n)
            out_write(out, cap, out_len, "(empty)\n");
        break;
    }
    case CMD_DBSIZE:
        out_write(out, cap, out_len, "(integer) %d\n", db->count);
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
        out_write(out, cap, out_len, "OK\n");
        break;
    }
    case CMD_HELP:
        out_write(out, cap, out_len,
                  "  SET    <key> <value>\n"
                  "  GET    <key>\n"
                  "  DEL    <key> [key ...]\n"
                  "  EXISTS <key>\n"
                  "  APPEND <key> <value>\n"
                  "  RENAME <old> <new>\n"
                  "  MSET   <k> <v> [k v ...]\n"
                  "  MGET   <key> [key ...]\n"
                  "  KEYS / DBSIZE / FLUSHDB\n"
                  "  EXIT / QUIT\n");
        break;
    case CMD_EXIT:
        return 1;
    default:
        break;
    }
    return 0;
}
