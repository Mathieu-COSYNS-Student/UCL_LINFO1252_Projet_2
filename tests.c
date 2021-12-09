#include <stdbool.h>
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

void assertEquals(int expected, int value, const char *const message)
{
    if (expected != value)
        fprintf(stderr, "%s - expected: %d, value: %d\n", message, expected, value);
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
    assertEquals(5, ret, "check_archive error");

    lseek(fd, 0L, SEEK_SET);
    ret = exists(fd, "ok/");
    assertEquals(1, ret, "exists error");

    lseek(fd, 0L, SEEK_SET);
    ret = exists(fd, "ok/ok_file.c");
    assertEquals(1, ret, "exists error");

    lseek(fd, 0L, SEEK_SET);
    ret = exists(fd, "nope");
    assertEquals(0, ret, "exists error");

    uint8_t buffer[4];
    size_t len = 4;
    size_t *len_ptr = &len;

    lseek(fd, 0L, SEEK_SET);
    ret = read_file(fd, "ok_long.txt", 512, buffer, len_ptr);
    assertEquals(0, ret, "read_file error");

    lseek(fd, 0L, SEEK_SET);
    ret = read_file(fd, "ok/ok_file.c", 4, buffer, len_ptr);
    assertEquals(0, ret, "read_file error");

    lseek(fd, 0L, SEEK_SET);
    ret = read_file(fd, "ok/ok_file2.c", 2, buffer, len_ptr);
    assertEquals(2, ret, "read_file error");

    lseek(fd, 0L, SEEK_SET);
    ret = read_file(fd, "ok_long.txt", 1000, buffer, len_ptr);
    assertEquals(-2, ret, "read_file error");

    lseek(fd, 0L, SEEK_SET);
    ret = read_file(fd, "ok/ok_file.c", 5, buffer, len_ptr);
    assertEquals(-2, ret, "read_file error");

    lseek(fd, 0L, SEEK_SET);
    ret = read_file(fd, "ok/ok_file2.c", 5, buffer, len_ptr);
    assertEquals(-2, ret, "read_file error");

    close(fd);

    return 0;
}