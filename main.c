#include "core/hash_table/hash-table.h"
#include "core/log.h"

#include "net/tcp/server.h"
#include "net/tcp/tcp.h"

#include <stdlib.h>

#define PORT   6379
#define BACKLOG 512

int main(void)
{
    HashTable* db = ht_create();

    int sd = create_tcp_socketd(AF_INET);
    if (sd < 0)
        return 1;

    int one = 1;
    int opt[2] = {SOL_SOCKET, SO_REUSEADDR};
    set_socket_opt(sd, opt, &one);

    if (tcp_bind(sd, PORT) != TCP_SUCCESS)
        return 1;
    if (tcp_listen(sd, BACKLOG) != TCP_SUCCESS)
        return 1;
    if (set_nonblocking(sd) != TCP_SUCCESS)
        return 1;

    LOG(stdout, "ykr-cache listening on port %d", PORT);

    server_run(sd, db);

    close(sd);
    ht_destroy(db);
    return 0;
}
