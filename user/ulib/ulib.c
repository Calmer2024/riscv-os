//
// Created by czh on 2025/12/13.
//

#include "user.h"

int
stat(const char *n, struct stat *st)
{
    int fd;
    int r;

    fd = open(n, O_RDONLY);
    if(fd < 0)
        return -1;
    r = fstat(fd, st);
    close(fd);
    return r;
}
