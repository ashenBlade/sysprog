#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "utils.h"

const char **extract_filenames(int argc, const char **argv, int *count)
{
    if (argc < 2)
    {
        fprintf(stderr, "Использование: %s <file1> <file2> ...\n", argv[0]);
        exit(1);
    }

    const char **filenames = (const char **)malloc(sizeof(char *) * (argc - 1));
    for (long i = 1; i < argc; i++)
    {
        const char *filename = argv[i];
        struct stat sb;
        int result = stat(filename, &sb);
        if (result == -1)
        {
            perror("stat");
            exit(1);
        }
        if (!S_ISREG(sb.st_mode))
        {
            fprintf(stderr, "%s - не является файлом\n", filename);
            exit(1);
        }

        filenames[i - 1] = filename;
    }
    *count = argc - 1;
    return filenames;
}

#define TEMP_FILE_MASK "/tmp/coro-sort-XXXXXX\0"

struct temp_file_struct
{
    char filename[sizeof(TEMP_FILE_MASK)];
    int fd;
};

temp_file_t *temp_file_new()
{
    temp_file_t *temp_file = (temp_file_t *)malloc(sizeof(temp_file_t));
    memcpy(&temp_file->filename, TEMP_FILE_MASK, sizeof(TEMP_FILE_MASK));
    int fd = mkstemp(temp_file->filename);
    if (fd == -1)
    {
        perror("open");
        exit(1);
    }

    temp_file->fd = fd;
    return temp_file;
}

int temp_file_fd(temp_file_t *temp_file)
{
    return temp_file->fd;
}

void temp_file_free(temp_file_t *temp_file)
{
    close(temp_file->fd);
    unlink(temp_file->filename);
    temp_file->fd = -1;
    free(temp_file);
}
