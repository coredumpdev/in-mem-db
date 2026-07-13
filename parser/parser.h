#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"
#include "../utils/log.h"


typedef struct 
{
    Lexer lexer;
    Token current;
}Parser;


static void parser_init(Parser*p, const char *src);
static Token parser_consume(Parser *p);
static Token parser_expect(Parser *p, TokenKind kind);
static CmdKind resolve_cmd(const char *s);
static void push_arg(ASTNode *node, const char *val);
static int parse_value(Parser *p, char *out);
static ASTNode *parse(Parser* p);



#endif