#ifndef AST_H
#define AST_H

#include <stdlib.h>
#include <string.h>

typedef enum {
    CMD_SET,
    CMD_GET,
    CMD_DEL,
    CMD_EXISTS,
    CMD_APPEND,
    CMD_RENAME,
    CMD_MSET,
    CMD_MGET,
    CMD_KEYS,
    CMD_DBSIZE,
    CMD_FLUSHDB,
    CMD_HELP,
    CMD_EXIT,
    CMD_UNKNOWN,
} CmdKind;

typedef struct ArgNode {
    char value[256];
    struct ArgNode* next;
} ArgNode;

typedef struct {
    CmdKind kind;
    ArgNode* args;
    int arg_count;
} ASTNode;

ArgNode* make_arg(const char* val);
void free_ast(ASTNode* node);

#endif
