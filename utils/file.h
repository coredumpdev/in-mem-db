#ifndef FILE_H
#define FILE_H

#include "../core/enums.h"
#include "../core/log.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int open_file(const char* file_name, int flag);
size_t read_file(int fd, char* out, size_t size);
size_t write_file(int fd, char* data, size_t size);
int close_file(int fd);
off_t seek(int fd, size_t pos);

#endif
