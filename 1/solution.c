#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "libcoro.h"
#include "utils.h"
#include 

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
    const char *filename;
} sort_context;

static struct sort_context *
sort_context_new(int id, const char *filename)
{
    struct sort_context *ctx = malloc(sizeof(*ctx));
    ctx->coroutine_id = id;
    ctx->filename = strdup(filename);
    return ctx;
}

static void
sort_context_delete(struct sort_context *ctx)
{
    free(ctx->filename);
    free(ctx);
}

/// @brief Функция для запуска алгоритма внешней сортировки файла
/// @param context Указатель на sort_context
/// @return 0 - сортировка произошла успешно, -1 - произошла ошибка
static int
sort_external_coro(void *context)
{
    struct coro *this = coro_this();
    sort_context *ctx = context;
    // void print_error_file_open(const char *filename)
    // {
    //     write(STDERR_FILENO, "Ошибка открытия файла: ", strlen("Ошибка открытия файла: "));
    //     write(STDERR_FILENO, filename, strlen(filename));
    //     write(STDERR_FILENO, "\n", sizeof("\n"));
    // }
    // int src_file_fd = open(src, O_RDONLY);
    // if (src_file_fd == -1)
    // {
    //     print_error_file_open(src);
    //     return 1;
    // }

    // int temp_file_fd = open(temp, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    // if (temp_file_fd == -1)
    // {
    //     close(src_file_fd);
    //     print_error_file_open(temp);
    //     return 1;
    // }

    return 0;
}

void global_init(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    coro_sched_init();
}

int main(int argc, char **argv)
{
    global_init(argc, argv);
    const char **filenames = extract_filenames(argc, argv);

    assert(filenames != NULL);

    /*
     * Запускаем корутины для каждого файла в списке
     */
    int id = 0;
    while (filenames[id] != NULL)
    {
        coro_new(sort_external_coro, sort_context_new(id, filenames[id]));
        ++id;
    }

    /* 
     * Запускаем корутины и ждем их завершения
     */
    struct coro *c;
    while ((c = coro_sched_wait()) != NULL)
    {
        /* TODO: добавить проверку кода результата. 
         * Если ошибка - завершаем работу и показываем сообщение об ошибке 
         */
        printf("Finished %d\n", coro_status(c));
        coro_delete(c);
    }

    /* Все корутины завершились - запускаем мерж всех файлов */

    /* TODO: слияние */

    return 0;
}
