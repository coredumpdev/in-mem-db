#include "utils/log.h"
#include "core/hash_table/hash-table.h"


#include "core/hash_table/hash-table.c"
#include "parser/ast.c"
#include "parser/lexer.c"
#include "parser/parser.c"
#include "parser/eval.c"

static int run(HashTable *db, const char *line)
{
    LOG(stdout, "ykr> %s", line);

    Parser p;
    parser_init(&p, line);

    ASTNode *node = parse(&p);
    if (!node) return 0;               

    int stop = eval(node, db);
    free_ast(node);
    free(node);
    return stop;
}


int main(void)
{
    HashTable *ht = ht_create();

  
    HT_SET_STR(ht, "name", strdup("Tolga"));
    HT_SET_STR(ht, "city", strdup("Istanbul"));
    HT_SET_STR(ht, "lang", strdup("tr"));

    LOG(stdout, "name -> %s", (char *) HT_GET_STR(ht, "name"));

    const char *program[] = {
        "SET greeting \"merhaba dunya\"",
        "GET greeting",
        "APPEND greeting \"!\"",
        "GET greeting",
        "MSET a 1 b 2 c 3",
        "MGET a b c",
        "EXISTS name",
        "DEL city lang",
        "DBSIZE",
        "RENAME name nick",
        "GET nick",
        "KEYS",
    };

    for (size_t i = 0; i < sizeof(program) / sizeof(program[0]); i++)
        run(ht, program[i]);

    ht_destroy(ht);
    return 0;
}
