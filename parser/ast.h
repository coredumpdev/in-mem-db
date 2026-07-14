#ifndef AST_H
#define AST_H

#include <stdlib.h>
#include <string.h>

#include "../core/enums.h"
typedef struct ArgNode
{
    char value[256];
    struct ArgNode* next;
} ArgNode;

typedef struct
{
    CmdKind kind;
    ArgNode* args;
    int arg_count;
} ASTNode;

ArgNode* make_arg(const char* val);
void free_ast(ASTNode* node);

#endif
