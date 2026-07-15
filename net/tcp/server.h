#ifndef SERVER_H
#define SERVER_H

#include "../../core/hash_table/hash-table.h"
#include <stdint.h>
#include <stdio.h>

#define QUERYBUF_CAP 65536
#define OUTBUF_CAP   65536

typedef struct Client
{
    int sd;
    char querybuf[QUERYBUF_CAP];
    size_t query_len;
    char outbuf[OUTBUF_CAP];
    size_t out_len;
    size_t out_sent;
    uint32_t events;
    int close_after_write;
} Client;

void server_run(int listen_sd, HashTable* db);
void accept_clients(int ep, int listen_sd);
void read_from_client(int ep, Client* c, HashTable* db);
void write_to_client(int ep, Client* c);
void close_client(int ep, Client* c);

#endif
