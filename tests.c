#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "lib_tar.h"

/**
 * You are free to use this file to write tests for your implementation
 */

void debug_dump(const uint8_t *bytes, size_t len)
{
    for (int i = 0; i < len;)
    {
        printf("%04x:  ", (int)i);

        for (int j = 0; j < 16 && i + j < len; j++)
        {
            printf("%02x ", bytes[i + j]);
        }
        printf("\t");
        for (int j = 0; j < 16 && i < len; j++, i++)
        {
            printf("%c ", bytes[i]);
        }
        printf("\n");
    }
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage: %s tar_file\n", argv[0]);
        return -1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1)
    {
        perror("open(tar_file)");
        return -1;
    }

    int ret;

    lseek(fd, 0L, SEEK_SET);
    ret = check_archive(fd);
    printf("check_archive returned %d\n", ret);

    lseek(fd, 0L, SEEK_SET);
    ret = exists(fd, "ok/");
    printf("exists returned %d\n", ret);

    lseek(fd, 0L, SEEK_SET);
    ret = exists(fd, "ok/ok_file.c");
    printf("exists returned %d\n", ret);

    lseek(fd, 0L, SEEK_SET);
    ret = exists(fd, "nope");
    printf("exists returned %d\n", ret);

    lseek(fd, 0L, SEEK_SET);
    uint8_t buffer[4];
    size_t len = 4;
    size_t *len_ptr = &len;
    ret = read_file(fd, "ok/ok_file.c", 1000, buffer, len_ptr);
    printf("read_file returned %d\n", ret);

    close(fd);

    return 0;
}