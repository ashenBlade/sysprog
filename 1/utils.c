#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "utils.h"

#define DEFAULT_LATENCY 100000

void print_usage(const char **argv);

void extract_program_args(int argc, const char **argv, prog_args_t *args)
{
    if (argc < 2)
    {
        print_usage(argv);
        exit(1);
    }

    if (strcmp(argv[1], "--help") == 0)
    {
        print_usage(argv);
        exit(0);
    }

    int i = 1;
    long long latency = DEFAULT_LATENCY;
    if (strcmp(argv[1], "-l") == 0 || strcmp(argv[1], "--latency") == 0)
    {
        if (argc <= 3)
        {
            /** Указана только задержка - без файлов */
            print_usage(argv);
            exit(1);
        }

        latency = strtoll(argv[2], NULL, 10);
        if (latency == 0)
        {
            printf("Задержка не может быть равна 0\n");
            exit(1);
        }

        if (latency < 0)
        {
            printf("Задержка не может быть отрицательной\n");
            exit(1);
        }

        i = 3;
    }

    int coro_count = -1;
    if (strcmp(argv[3], "-c") == 0 || strcmp(argv[3], "--latency") == 0)
    {
        if (argc <= 5)
        {
            print_usage(argv);
            exit(1);
        }

        coro_count = (int)strtol(argv[4], NULL, 10);
        if (coro_count == 0)
        {
            printf("Количество корутин должно быть положительным. Передано 0\n");
            exit(1);
        }

        if (coro_count < 0)
        {
            printf("Количество корутин должно быть положительным. Передано %d\n", coro_count);
            exit(1);
        }

        i = 5;
    }

    int files_count = argc - i;
    const char **filenames = (const char **)malloc(sizeof(char *) * files_count);
    for (int f = 0; f < files_count; f++)
    {
        const char *filename = argv[f + i];
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

        filenames[f] = filename;
    }
    args->filenames = filenames;
    args->files_count = files_count;
    args->latency_us = latency;
    args->coro_count = coro_count == -1
                           ? files_count
                           : coro_count;
}

void print_usage(const char **argv)
{
    printf("Использование: %s [-l|--latency LATENCY] [-c|--coro-count CORO_COUNT] <file1> <file2> ...\n", argv[0]);
    printf("\t-l|--latency LATENCY - указать задержку в мкс. Если не указано, будет выставлено в 100000 (100мс)\n");
    printf("\t-c|--coro-count CORO_COUNT - указать количество корутин, которое нужно использовать. Если не указано - равняется количеству переданных файлов\n");
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
