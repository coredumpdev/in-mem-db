#include "ast.h"

ArgNode* make_arg(const char* val) {
    ArgNode* a = calloc(1, sizeof(ArgNode));
    strncpy(a->value, val, 255);

    return a;
}

void free_ast(ASTNode* node) {
    ArgNode* a = node->args;

    while (a) {
        ArgNode* n = a->next;
        free(a);
        a = n;
    }
}
