#ifndef PARSER_H
#define PARSER_H

#include "../utils/log.h"
#include "ast.h"
#include "lexer.h"

typedef struct {
    Lexer lexer;
    Token current;
} Parser;

void parser_init(Parser* p, const char* src);
ASTNode* parse(Parser* p);

#endif
