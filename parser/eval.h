#ifndef EVAL_H
#define EVAL_H

#include "../core/hash_table/hash-table.h"
#include "ast.h"

int eval(ASTNode* node, HashTable* db);

#endif
