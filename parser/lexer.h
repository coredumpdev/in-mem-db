#ifndef LEXER_H
#define LEXER_H

#include <ctype.h>
#include <string.h>

typedef enum
{
    TK_IDENT,
    TK_STRING,
    TK_EOF,
    TK_ERROR
} TokenKind;

typedef struct
{
    TokenKind kind;
    char text[256];
} Token;

typedef struct
{
    const char* src;
    int pos;
} Lexer;

void lexer_init(Lexer* l, const char* src);
Token next_token(Lexer* l);

#endif
