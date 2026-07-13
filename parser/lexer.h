#ifndef LEXER_H
#define LEXER_H

#include <string.h>
#include <ctype.h>


typedef enum {
    TK_IDENT,
    TK_STRING,
    TK_EOF,
    TK_ERROR
}TokenKind;


typedef struct {
    TokenKind   kind;
    char        text[256];
}Token;


typedef struct {
    const char *src;
    int         pos;
}Lexer;


static void lexer_init(Lexer *l, const char *src);
static char peek(Lexer *l);
static char advance(Lexer *l);
static void skip_ws(Lexer *l);


static Token next_token(Lexer *l);

#endif
