#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/param.h>

#include "lib_tar.h"

__off_t get_end_of_file_empty_blocks_pos(int fd)
{
    __off_t current_pos = lseek(fd, 0L, SEEK_CUR);

    __off_t empty_block_pos = lseek(fd, 0L, SEEK_END);
    bool is_empty_block = true;

    do
    {
        __off_t new_empty_block_pos = lseek(fd, empty_block_pos - BLOCKSIZE, SEEK_SET);

        if (new_empty_block_pos > 0)
        {
            uint8_t buffer[BLOCKSIZE / 8] = {0};
            int bytes_read;

            do
            {
                bytes_read = read(fd, buffer, sizeof(buffer));

                for (size_t i = 0; i < BLOCKSIZE / 8; i++)
                {
                    if (buffer[i] != 0)
                    {
                        is_empty_block = false;
                        break;
                    }
                }

            } while (bytes_read && bytes_read != -1);

            if (is_empty_block)
                empty_block_pos = new_empty_block_pos;
        }
        else
        {
            is_empty_block = false;
        }
    } while (is_empty_block);

    lseek(fd, current_pos, SEEK_SET);

    return empty_block_pos;
}

bool read_header(int tar_fd, tar_header_t *const header)
{
    if (read(tar_fd, header, sizeof(tar_header_t)) != sizeof(tar_header_t))
        return false;

    return true;
}

bool seek_and_read_header(int tar_fd, tar_header_t *const header, __off_t end)
{
    int block_file_size = TAR_INT(header->size);

    if (block_file_size % 512 != 0)
        block_file_size += (512 - block_file_size % 512);

    __off_t offset = lseek(tar_fd, block_file_size, SEEK_CUR);

    if (offset == -1)
        return false;

    if (offset >= end)
        return false;

    return read_header(tar_fd, header);
}

/**
 * Checks whether the archive is valid.
 *
 * Each non-null header of a valid archive has:
 *  - a magic value of "ustar" and a null,
 *  - a version value of "00" and no null,
 *  - a correct checksum
 *
 * @param tar_fd A file descriptor pointing to the start of a file supposed to contain a tar archive.
 *
 * @return a zero or positive value if the archive is valid, representing the number of non-null headers in the archive,
 *         -1 if the archive contains a header with an invalid magic value,
 *         -2 if the archive contains a header with an invalid version value,
 *         -3 if the archive contains a header with an invalid checksum value
 */
int check_archive(int tar_fd)
{
    tar_header_t tar_header;
    __off_t end_of_file_empty_blocks_pos = get_end_of_file_empty_blocks_pos(tar_fd);
    int header_count = 0;

    read_header(tar_fd, &tar_header);

    do
    {
        if (strncmp(tar_header.magic, TMAGIC, TMAGLEN))
            return -1;

        if (strncmp(tar_header.version, TVERSION, TVERSLEN))
            return -2;

        char tar_header_chksum[8];
        for (size_t i = 0; i < 8; i++)
        {
            tar_header_chksum[i] = tar_header.chksum[i];
            tar_header.chksum[i] = ' ';
        }

        unsigned int chksum = 0;

        uint8_t *tar_header_bytes_ptr = (uint8_t *)&tar_header;

        for (size_t i = 0; i < sizeof(tar_header_t); i++)
        {
            chksum += *tar_header_bytes_ptr;
            tar_header_bytes_ptr++;
        }

        if (TAR_INT(tar_header_chksum) != chksum)
            return -3;

        header_count++;

    } while (seek_and_read_header(tar_fd, &tar_header, end_of_file_empty_blocks_pos));

    return header_count;
}

bool exists_h(int tar_fd, const char *const path, tar_header_t *const tar_header)
{
    __off_t end_of_file_empty_blocks_pos = get_end_of_file_empty_blocks_pos(tar_fd);
    read_header(tar_fd, tar_header);

    do
    {
        if (!strncmp(tar_header->name, path, NAMELEN))
        {
            return true;
        }
    } while (seek_and_read_header(tar_fd, tar_header, end_of_file_empty_blocks_pos));

    return false;
}

/**
 * Checks whether an entry exists in the archive.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive,
 *         any other value otherwise.
 */
int exists(int tar_fd, char *path)
{
    tar_header_t tar_header;
    return exists_h(tar_fd, path, &tar_header);
}

bool check_is_dir(const tar_header_t *const tar_header)
{
    return tar_header->typeflag == DIRTYPE;
}

/**
 * Checks whether an entry exists in the archive and is a directory.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive or the entry is not a directory,
 *         any other value otherwise.
 */
int is_dir(int tar_fd, char *path)
{
    tar_header_t tar_header;
    return exists_h(tar_fd, path, &tar_header) && check_is_dir(&tar_header);
}

bool check_is_file(const tar_header_t *const tar_header)
{
    return tar_header->typeflag == REGTYPE || tar_header->typeflag == AREGTYPE;
}

/**
 * Checks whether an entry exists in the archive and is a file.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive or the entry is not a file,
 *         any other value otherwise.
 */
int is_file(int tar_fd, char *path)
{
    tar_header_t tar_header;
    return exists_h(tar_fd, path, &tar_header) && check_is_file(&tar_header);
}

bool check_is_symlink(const tar_header_t *const tar_header)
{
    return tar_header->typeflag == LNKTYPE || tar_header->typeflag == SYMTYPE;
}

/**
 * Checks whether an entry exists in the archive and is a symlink.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 * @return zero if no entry at the given path exists in the archive or the entry is not symlink,
 *         any other value otherwise.
 */
int is_symlink(int tar_fd, char *path)
{
    tar_header_t tar_header;
    return exists_h(tar_fd, path, &tar_header) && check_is_symlink(&tar_header);
}

/**
 * Lists the entries at a given path in the archive.
 * list() does not recurse into the directories listed at the given path.
 *
 * Example:
 *  dir/          list(..., "dir/", ...) lists "dir/a", "dir/b", "dir/c/" and "dir/e/"
 *   ├── a
 *   ├── b
 *   ├── c/
 *   │   └── d
 *   └── e/
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive. If the entry is a symlink, it must be resolved to its linked-to entry.
 * @param entries An array of char arrays, each one is long enough to contain a tar entry path.
 * @param no_entries An in-out argument.
 *                   The caller set it to the number of entries in `entries`.
 *                   The callee set it to the number of entries listed.
 *
 * @return zero if no directory at the given path exists in the archive,
 *         any other value otherwise.
 */
int list(int tar_fd, char *path, char **entries, size_t *no_entries)
{
    tar_header_t tar_header;

    if (!exists_h(tar_fd, path, &tar_header))
    {
        return 0;
    }

    if (check_is_symlink(&tar_header))
    {
        int linkname_len = strnlen(tar_header.linkname, NAMELEN);
        tar_header.linkname[linkname_len] = '/';
        lseek(tar_fd, 0L, SEEK_SET);
        return list(tar_fd, tar_header.linkname, entries, no_entries);
    }

    if (!check_is_dir(&tar_header))
        return 0;

    if (*no_entries == 0)
        return 1;

    lseek(tar_fd, 0L, SEEK_SET);

    __off_t end_of_file_empty_blocks_pos = get_end_of_file_empty_blocks_pos(tar_fd);
    read_header(tar_fd, &tar_header);

    int path_len = strnlen(path, NAMELEN);

    int no_entries_read = 0;

    do
    {
        if (!strncmp(path, tar_header.name, path_len) &&
            strncmp(path, tar_header.name, NAMELEN))
        {
            int name_len = strnlen(tar_header.name, NAMELEN);
            bool not_subdir = true;
            for (size_t i = path_len + 1; i < name_len; i++)
            {
                if (tar_header.name[i - 1] == '/')
                {
                    not_subdir = false;
                    break;
                }
            }

            if (not_subdir)
                strcpy(entries[no_entries_read++], tar_header.name);
        }
    } while (no_entries_read < *no_entries && seek_and_read_header(tar_fd, &tar_header, end_of_file_empty_blocks_pos));

    *no_entries = no_entries_read;

    return 1;
}

/**
 * Reads a file at a given path in the archive.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive to read from.  If the entry is a symlink, it must be resolved to its linked-to entry.
 * @param offset An offset in the file from which to start reading from, zero indicates the start of the file.
 * @param dest A destination buffer to read the given file into.
 * @param len An in-out argument.
 *            The caller set it to the size of dest.
 *            The callee set it to the number of bytes written to dest.
 *
 * @return -1 if no entry at the given path exists in the archive or the entry is not a file,
 *         -2 if the offset is outside the file total length,
 *         zero if the file was read in its entirety into the destination buffer,
 *         a positive value if the file was partially read, representing the remaining bytes left to be read to reach
 *         the end of the file.
 *
 */
ssize_t read_file(int tar_fd, char *path, size_t offset, uint8_t *dest, size_t *len)
{
    if (lseek(tar_fd, 0L, SEEK_END) < offset)
        return -2;

    lseek(tar_fd, 0L, SEEK_SET);

    tar_header_t tar_header;

    if (!exists_h(tar_fd, path, &tar_header))
        return -1;

    if (check_is_symlink(&tar_header))
    {
        lseek(tar_fd, 0L, SEEK_SET);
        return read_file(tar_fd, tar_header.linkname, offset, dest, len);
    }

    if (!check_is_file(&tar_header))
        return -1;

    long size = TAR_INT(tar_header.size);

    if (offset > size)
        return -2;

    lseek(tar_fd, offset, SEEK_CUR);
    ssize_t nb_read = read(tar_fd, dest, MIN(*len, size - offset));

    if (nb_read == -1)
    {
        perror("read_file: ");
        exit(EXIT_FAILURE);
    }

    *len = nb_read;

    return size - offset - nb_read;
}