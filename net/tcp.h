#ifndef TCP_H
#define TCP_H

#include "../globals/enums.h"
#include "../utils/log.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int create_tcp_socketd(int domain);
int set_socket_opt(int sd, int flag[2], int* opt);
int tcp_bind(int sd, unsigned short port);
int tcp_listen(int sd, int queue);

#endif
