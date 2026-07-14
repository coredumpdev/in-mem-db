#ifndef FILE_H
#define FILE_H

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include "../globals/enums.h"
#include "log.h"

int open_file(const char* file_name, int flag);
size_t read_file(int fd, char *out, size_t size);
size_t write_file(int fd, char *data, size_t size);
int close_file(int fd);
off_t seek(int fd, size_t pos);


#endif
