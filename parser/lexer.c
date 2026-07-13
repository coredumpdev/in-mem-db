
#include "lexer.h"


static void lexer_init(Lexer *l, const char *src) { l->src = src; l->pos = 0; }
static char peek(Lexer *l) { return l->src[l->pos]; }
static char advance(Lexer *l) { return l->src[l->pos++]; }
static void skip_ws(Lexer *l) { while(isspace((unsigned char) peek(l))) advance(l); }


static Token next_token(Lexer *l)
{
    Token t = {0};

    skip_ws(l);

    if (!peek(l))
    {
        t.kind = TK_EOF;
        return t;
    }

    if (peek(l) == '"') {
        advance(l);
        int i = 0;
        while(peek(l) && peek(l) != '"' && i < 256)
            t.text[i++] = advance(l);

        if (peek(l) == '"') advance(l);
        t.text[i] = '\0';

        t.kind = TK_STRING;
        return t;
    }

    if (isgraph((unsigned char) peek(l))) {
        int i = 0;
        while(isgraph((unsigned char) peek(l)) && peek(l) != '"' && i < 255)
        {
            t.text[i++] = advance(l);
        }
        t.text[i] = '\0';
        t.kind = TK_IDENT;
        return t;
    }


    t.kind = TK_ERROR;
    t.text[0] = advance(l);
    t.text[1] = '\0';
    return t;
}
