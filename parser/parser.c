#include "parser.h"
#include <stdio.h>

void parser_init(Parser* p, const char* src) {
    lexer_init(&p->lexer, src);
    p->current = next_token(&p->lexer);
}

static Token parser_consume(Parser* p) {
    Token t = p->current;
    p->current = next_token(&p->lexer);
    return t;
}

static Token parser_expect(Parser* p, TokenKind kind) {
    if (p->current.kind == kind)
        return parser_consume(p);
    Token err = {TK_ERROR, "unexpected token"};
    return err;
}

static CmdKind resolve_cmd(const char* s) {
    if (!strcasecmp(s, "SET"))
        return CMD_SET;
    if (!strcasecmp(s, "GET"))
        return CMD_GET;
    if (!strcasecmp(s, "DEL"))
        return CMD_DEL;
    if (!strcasecmp(s, "EXISTS"))
        return CMD_EXISTS;
    if (!strcasecmp(s, "APPEND"))
        return CMD_APPEND;
    if (!strcasecmp(s, "RENAME"))
        return CMD_RENAME;
    if (!strcasecmp(s, "MSET"))
        return CMD_MSET;
    if (!strcasecmp(s, "MGET"))
        return CMD_MGET;
    if (!strcasecmp(s, "KEYS"))
        return CMD_KEYS;
    if (!strcasecmp(s, "DBSIZE"))
        return CMD_DBSIZE;
    if (!strcasecmp(s, "FLUSHDB"))
        return CMD_FLUSHDB;
    if (!strcasecmp(s, "HELP"))
        return CMD_HELP;
    if (!strcasecmp(s, "EXIT") || !strcasecmp(s, "QUIT"))
        return CMD_EXIT;
    return CMD_UNKNOWN;
}

static void push_arg(ASTNode* node, const char* val) {
    ArgNode* a = make_arg(val);
    if (!node->args) {
        node->args = a;
    } else {
        ArgNode* cur = node->args;
        while (cur->next)
            cur = cur->next;
        cur->next = a;
    }

    node->arg_count++;
}

static int parse_value(Parser* p, char* out) {
    Token t = p->current;
    if (t.kind == TK_IDENT || t.kind == TK_STRING) {
        parser_consume(p);
        strncpy(out, t.text, 255);
        return 1;
    }
    return 0;
}

ASTNode* parse(Parser* p) {
    Token cmd_tok = parser_expect(p, TK_IDENT);
    if (cmd_tok.kind == TK_ERROR) {
        printf("ERR expected command, got '%s'\n", cmd_tok.text);
        return NULL;
    }

    ASTNode* node = calloc(1, sizeof(ASTNode));
    node->kind = resolve_cmd(cmd_tok.text);

    if (node->kind == CMD_UNKNOWN) {
        printf("ERR unknown command '%s'\n", cmd_tok.text);
        free(node);
        return NULL;
    }

    char val[256];
    while (parse_value(p, val))
        push_arg(node, val);

    int n = node->arg_count, err = 0;
    switch (node->kind) {
    case CMD_SET:
        if (n != 2) {
            puts("ERR SET <key> <value>");
            err = 1;
        }
        break;
    case CMD_GET:
        if (n != 1) {
            puts("ERR GET <key>");
            err = 1;
        }
        break;
    case CMD_DEL:
        if (n < 1) {
            puts("ERR DEL <key> [key ...]");
            err = 1;
        }
        break;
    case CMD_EXISTS:
        if (n != 1) {
            puts("ERR EXISTS <key>");
            err = 1;
        }
        break;
    case CMD_APPEND:
        if (n != 2) {
            puts("ERR APPEND <key> <value>");
            err = 1;
        }
        break;
    case CMD_RENAME:
        if (n != 2) {
            puts("ERR RENAME <old> <new>");
            err = 1;
        }
        break;
    case CMD_MSET:
        if (n < 2 || n % 2 != 0) {
            puts("ERR MSET k v [k v ...]");
            err = 1;
        }
        break;
    case CMD_MGET:
        if (n < 1) {
            puts("ERR MGET <key> [key ...]");
            err = 1;
        }
        break;
    default:
        break;
    }

    if (err) {
        free_ast(node);
        return NULL;
    }
    return node;
}
