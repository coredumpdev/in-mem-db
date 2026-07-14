#include "file.h"

int open_file(const char* file_name, int flag)
{
    int fd = open(file_name, flag);
    if (fd == -1)
    {
        LOG(stderr, "Couldn't find file %s\n", file_name);
        return FILE_NOT_FOUND;
    }
    return fd;
}

size_t read_file(int fd, char* out, size_t size)
{
    size_t n_read = read(fd, out, size);
    if (n_read <= 0)
    {
        close_file(fd);
        LOG(stderr, "Read error\n");
        return -1;
    }
    return n_read;
}
size_t write_file(int fd, char* data, size_t size)
{
    size_t n_write = write(fd, data, size);
    if (n_write == -1)
    {
        if (errno == EACCES)
        {
            LOG(stderr, "Permission denied\n");
            return -1;
        }
        return -1;
    }
    return n_write;
}

int close_file(int fd)
{
    if (close(fd) == -1)
    {
        return FILE_NOT_FOUND;
    }
    return FILE_SUCCESS;
}

off_t seek(int fd, size_t pos)
{
    return lseek(fd, pos, SEEK_SET);
}
