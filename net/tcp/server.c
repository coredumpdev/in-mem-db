#include "server.h"
#include "core/log.h"
#include "parser/ast.h"
#include "parser/eval.h"
#include "parser/parser.h"
#include "tcp.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_EVENTS 1024
#define LINE_CAP   512

static void mod_events(int ep, Client* c, uint32_t events)
{
    if (c->events == events)
        return;

    struct epoll_event ev = {.events = events, .data.ptr = c};
    if (epoll_ctl(ep, EPOLL_CTL_MOD, c->sd, &ev) == -1)
    {
        LOG(stderr, "epoll_ctl MOD error sd=%d", c->sd);
        return;
    }
    c->events = events;
}

void close_client(int ep, Client* c)
{
    epoll_ctl(ep, EPOLL_CTL_DEL, c->sd, NULL);
    close(c->sd);
    free(c);
}

void accept_clients(int ep, int listen_sd)
{
    for (;;)
    {
        int cfd = accept(listen_sd, NULL, NULL);
        if (cfd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            if (errno == EINTR)
                continue;
            LOG(stderr, "Accept error");
            break;
        }

        if (set_nonblocking(cfd) != TCP_SUCCESS)
        {
            LOG(stderr, "Couldn't set client nonblocking, sd=%d", cfd);
            close(cfd);
            continue;
        }

        Client* c = calloc(1, sizeof(Client));
        if (!c)
        {
            LOG(stderr, "Out of memory, dropping client sd=%d", cfd);
            close(cfd);
            continue;
        }
        c->sd = cfd;
        c->events = EPOLLIN;

        struct epoll_event ev = {.events = c->events, .data.ptr = c};
        if (epoll_ctl(ep, EPOLL_CTL_ADD, cfd, &ev) == -1)
        {
            LOG(stderr, "epoll_ctl ADD error sd=%d", cfd);
            free(c);
            close(cfd);
            continue;
        }

        LOG(stdout, "Client connected sd=%d", cfd);
    }
}

static int process_command(Client* c, const char* line, HashTable* db)
{
    Parser p;
    parser_init(&p, line);

    ASTNode* node = parse(&p, c->outbuf, OUTBUF_CAP, &c->out_len);
    if (!node)
        return 0;

    int stop = eval(node, db, c->outbuf, OUTBUF_CAP, &c->out_len);
    free_ast(node);
    free(node);
    return stop;
}

void write_to_client(int ep, Client* c)
{
    while (c->out_sent < c->out_len)
    {
        ssize_t n = send(c->sd, c->outbuf + c->out_sent, c->out_len - c->out_sent, MSG_NOSIGNAL);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                mod_events(ep, c, EPOLLIN | EPOLLOUT);
                return;
            }
            LOG(stderr, "Send error sd=%d", c->sd);
            close_client(ep, c);
            return;
        }
        c->out_sent += (size_t) n;
    }

    c->out_len = c->out_sent = 0;

    if (c->close_after_write)
    {
        LOG(stdout, "Client quit sd=%d", c->sd);
        close_client(ep, c);
        return;
    }
    mod_events(ep, c, EPOLLIN);
}

void read_from_client(int ep, Client* c, HashTable* db)
{
    if (c->query_len >= QUERYBUF_CAP)
    {
        LOG(stderr, "Protocol error: too big inline request, sd=%d", c->sd);
        close_client(ep, c);
        return;
    }

    ssize_t n = recv(c->sd, c->querybuf + c->query_len, QUERYBUF_CAP - c->query_len, 0);
    if (n == 0)
    {
        LOG(stdout, "Client disconnected sd=%d", c->sd);
        close_client(ep, c);
        return;
    }
    if (n < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            return;
        LOG(stderr, "Recv error sd=%d", c->sd);
        close_client(ep, c);
        return;
    }
    c->query_len += (size_t) n;

    int stop = 0;
    size_t start = 0;
    for (size_t i = 0; i < c->query_len; i++)
    {
        if (c->querybuf[i] != '\n')
            continue;

        size_t len = i - start;
        if (len && c->querybuf[start + len - 1] == '\r')
            len--;

        char line[LINE_CAP];
        if (len >= sizeof(line))
            len = sizeof(line) - 1;
        memcpy(line, c->querybuf + start, len);
        line[len] = '\0';

        start = i + 1;
        if (len == 0)
            continue;

        if (process_command(c, line, db))
        {
            stop = 1;
            break;
        }
    }

    memmove(c->querybuf, c->querybuf + start, c->query_len - start);
    c->query_len -= start;

    if (stop)
    {
        c->query_len = 0;
        c->close_after_write = 1;
    }

    if (c->out_sent < c->out_len)
        write_to_client(ep, c);
    else if (c->close_after_write)
        close_client(ep, c);
}

void server_run(int listen_sd, HashTable* db)
{
    int ep = epoll_create1(0);
    if (ep == -1)
    {
        LOG(stderr, "epoll_create1 error");
        return;
    }

    struct epoll_event ev = {.events = EPOLLIN, .data.ptr = NULL};
    if (epoll_ctl(ep, EPOLL_CTL_ADD, listen_sd, &ev) == -1)
    {
        LOG(stderr, "epoll_ctl ADD error for listen sd");
        close(ep);
        return;
    }

    struct epoll_event events[MAX_EVENTS];
    for (;;)
    {
        int n = epoll_wait(ep, events, MAX_EVENTS, -1);
        if (n == -1)
        {
            if (errno == EINTR)
                continue;
            LOG(stderr, "epoll_wait error");
            break;
        }

        for (int i = 0; i < n; i++)
        {
            if (events[i].data.ptr == NULL)
            {
                accept_clients(ep, listen_sd);
                continue;
            }

            Client* c = events[i].data.ptr;
            if (events[i].events & (EPOLLHUP | EPOLLERR))
            {
                close_client(ep, c);
                continue;
            }
            if (events[i].events & EPOLLIN)
            {
                read_from_client(ep, c, db);
                continue;
            }
            if (events[i].events & EPOLLOUT)
                write_to_client(ep, c);
        }
    }

    close(ep);
}
