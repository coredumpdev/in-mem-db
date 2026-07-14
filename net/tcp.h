#ifndef TCP_H
#define TCP_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "../utils/log.h"
#include "../globals/enums.h"

int create_tcp_socketd(int domain);
int set_socket_opt(int sd, int flag[2], int* opt);
int tcp_bind(int sd, unsigned short port);
int tcp_listen(int sd, int queue);


#endif
