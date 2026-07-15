#ifndef EVAL_H
#define EVAL_H

#include "../core/hash_table/hash-table.h"
#include "ast.h"

int eval(ASTNode* node, HashTable* db, char* out, size_t cap, size_t* out_len);

#endif
