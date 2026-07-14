#include "tcp.h"
#include <fcntl.h>

int create_tcp_socketd(int domain)
{
    int sd = socket(domain, SOCK_STREAM, 0);
    if (sd == -1)
    {
        LOG(stderr, "Couldn't create socket descriptor\n");
        return TCP_DESC_ERR;
    }

    return sd;
}

int set_socket_opt(int sd, int flag[2], int* opt)
{
    if (setsockopt(sd, flag[0], flag[1], opt, sizeof(int)) == -1)
    {
        LOG(stderr, "Couldn't set socket opt\n");
        return TCP_SOPT_ERR;
    }
    return TCP_SUCCESS;
}

int set_nonblocking(int sd)
{
    int flags = fcntl(sd, F_GETFL, 0);
    if (flags == -1)
        return TCP_SOPT_ERR;
    if (fcntl(sd, F_SETFL, flags | O_NONBLOCK) == -1)
        return TCP_SOPT_ERR;
    return TCP_SUCCESS;
}

int tcp_bind(int sd, unsigned short port)
{
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sd, (struct sockaddr*) &addr, sizeof addr) == -1)
    {
        LOG(stderr, "Bind error\n");
        return TCP_BIND_ERR;
    }

    return TCP_SUCCESS;
}

int tcp_listen(int sd, int queue)
{
    if (listen(sd, queue) == -1)
    {
        LOG(stderr, "Listen error\n");
        return TCP_LIST_ERR;
    }

    return TCP_SUCCESS;
}
