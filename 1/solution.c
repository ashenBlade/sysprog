#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "libcoro.h"
#include "utils.h"
#include "external_sort.h"
#include "merge_files.h"

/**
 * You can compile and run this code using the commands:
 *
 * $> gcc solution.c libcoro.c
 * $> ./a.out
 */

typedef struct sort_context
{
    /// @brief ID корутины, которая выполняется
    int coroutine_id;

    /// @brief Название файла для сортировки
    char *filename;

    /**
     * @brief Дескриптор исходного файла
     */
    int fd;

    /**
     * @brief Временный файл, в который необходимо сохранять отсортированные значения
     */
    temp_file_t *temp_file;
} sort_context;

static void
sort_context_init(struct sort_context *ctx, int id, const char *filename)
{
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {

        perror("open");
        exit(1);
    }

    ctx->temp_file = temp_file_new();
    ctx->coroutine_id = id;
    ctx->filename = strdup(filename);
    ctx->fd = fd;
}

static void
sort_context_free(struct sort_context *ctx)
{
    close(ctx->fd);
    temp_file_free(ctx->temp_file);
    free(ctx->filename);
}

/// @brief Функция для запуска алгоритма внешней сортировки файла
/// @param context Указатель на sort_context
/// @return 0 - сортировка произошла успешно, -1 - произошла ошибка
static int
sort_external_coro(void *context)
{
    struct coro *this = coro_this();
    (void)this;
    sort_context *ctx = context;

    sort_file_external_coro(ctx->fd, temp_file_fd(ctx->temp_file));

    return 0;
}

void global_init(int argc, const char **argv)
{
    (void)argc;
    (void)argv;

    coro_sched_init();
}

int main(int argc, const char **argv)
{
    global_init(argc, argv);
    int files_count;
    const char **filenames = extract_filenames(argc, argv, &files_count);
    assert(filenames != NULL);

    /*
     * Запускаем корутины для каждого файла в списке
     */
    sort_context *contexts = (sort_context *)malloc(sizeof(sort_context) * files_count);
    for (long i = 0; i < files_count; i++)
    {
        /* code */
        sort_context *cur_ctx = &contexts[i];
        sort_context_init(cur_ctx, i, filenames[i]);
        coro_new(sort_external_coro, cur_ctx);
    }

    /*
     * Запускаем корутины и ждем их завершения
     */
    struct coro *c;
    while ((c = coro_sched_wait()) != NULL)
    {
        printf("Finished %d\n", coro_status(c));
        coro_delete(c);
    }

    /* Все корутины завершились - запускаем мерж всех файлов */

    int *fds = (int *)malloc(sizeof(int) * files_count);
    for (long i = 0; i < files_count; i++)
    {
        int temp_fd = temp_file_fd(contexts[i].temp_file);
        if (lseek(temp_fd, 0, SEEK_SET) == -1)
        {
            perror("lseek");
            exit(1);
        }

        fds[i] = temp_fd;
    }

    int result_fd = open("result.txt",
                         O_CREAT | O_RDWR | O_APPEND | O_TRUNC,
                         S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    if (result_fd == -1)
    {
        perror("open");
        exit(1);
    }

    merge_files(result_fd, fds, files_count);

    close(result_fd);

    for (long i = 0; i < files_count; i++)
    {
        sort_context_free(&contexts[i]);
    }
    free(fds);
    free(contexts);
    free(filenames);
    return 0;
}
