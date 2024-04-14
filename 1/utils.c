#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

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

#define TEMP_FILENAME_MASK "/tmp/coro-sort.XXXXXX"

void temp_file_init(temp_file_t *temp_file)
{
    char *buf = (char *)malloc(strlen(TEMP_FILENAME_MASK) * sizeof(char));
    memcpy(buf, TEMP_FILENAME_MASK, sizeof(TEMP_FILENAME_MASK));
    int fd = mkstemp(buf);
    if (fd == -1)
    {
        perror("mkstemp");
        exit(1);
    }

    temp_file->filename = buf;
    temp_file->fd = fd;
}

void temp_file_free(temp_file_t *temp_file)
{
    if (temp_file->filename == NULL)
    {
        return;
    }
    close(temp_file->fd);
    unlink(temp_file->filename);
    free(temp_file->filename);
    temp_file->filename = NULL;
    temp_file->fd = -1;
}
