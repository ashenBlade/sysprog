#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include "libcoro.h"
#include "utils.h"
#include "external_sort.h"
#include "merge_files.h"
#include "timespec_helpers.h"

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

static void 
init_coro(prog_args_t *args)
{
    struct timespec latency;
    us_to_timespec(args->latency_us, &latency);
    struct timespec coro_lat;
    timespec_div(&latency, args->files_count, &coro_lat);
    fprintf(stderr, "Рассчитанная задержка корутин: %lld с, %lld нс\n", (long long)coro_lat.tv_sec, (long long)coro_lat.tv_nsec);
    coro_sched_init(&coro_lat);
}

static void display_coro_stats(struct coro *c)
{
    coro_stats_t stats;
    coro_stats(c, &stats);
    printf("Корутина завершилась:\n\tВремя работы: %lld с, %lld нс\n\tПереключений контекста: %lld\n\tЛожных переключений контекста: %lld\n", (long long)stats.worktime.tv_sec, (long long)stats.worktime.tv_nsec, (long long) stats.switch_count, (long long) stats.false_switch_count);
}

static void
display_work_time(struct timespec *start, struct timespec *end)
{
    struct timespec diff;
    timespec_sub(end, start, &diff);
    printf("Общее время работы: %lld с, %lld нс\n", (long long)diff.tv_sec, (long long)diff.tv_nsec);
}

int main(int argc, const char **argv)
{
    prog_args_t args;
    extract_program_args(argc, argv, &args);
    init_coro(&args);

    /*
     * Запускаем корутины для каждого файла в списке
     */
    sort_context *contexts = (sort_context *)malloc(sizeof(sort_context) * args.files_count);
    for (long i = 0; i < args.files_count; i++)
    {
        /* code */
        sort_context *cur_ctx = &contexts[i];
        sort_context_init(cur_ctx, i, args.filenames[i]);
        coro_new(sort_external_coro, cur_ctx);
    }

    struct timespec start_time;
    clock_gettime(CLOCK_REALTIME, &start_time);

    /*
     * Запускаем корутины и ждем их завершения
     */
    struct coro *c;
    while ((c = coro_sched_wait()) != NULL)
    {
        display_coro_stats(c);
        coro_delete(c);
    }

    /* Все корутины завершились - запускаем мерж всех файлов */

    int *fds = (int *)malloc(sizeof(int) * args.files_count);
    for (long i = 0; i < args.files_count; i++)
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

    merge_files(result_fd, fds, args.files_count);

    struct timespec end_time;
    clock_gettime(CLOCK_REALTIME, &end_time);
    display_work_time(&start_time, &end_time);

    close(result_fd);
    for (long i = 0; i < args.files_count; i++)
    {
        sort_context_free(&contexts[i]);
    }
    free(fds);
    free(contexts);
    free(args.filenames);
    return 0;
}
