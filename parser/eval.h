#ifndef EVAL_H
#define EVAL_H

#include "ast.h"
#include "../core/hash_table/hash-table.h"

static const char *arg(ASTNode *node, int n);
static int eval(ASTNode *node, HashTable *db);

#endif