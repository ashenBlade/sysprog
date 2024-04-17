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
#include "stack.h"

/**
 * You can compile and run this code using the commands:
 *
 * $> gcc solution.c libcoro.c
 * $> ./a.out
 */

typedef struct coro_sort_context
{
    /**
     * @brief ID корутины, которая выполняется
     */
    int coroutine_id;

    /**
     * @brief Стек из файлов, которые необходимо отсортировать
     */
    stack_t *files;
} coro_sort_context_t;

/** Единица, участвующая в сортировке */
typedef struct sort_element
{
    /**
     * @brief
     */
    char *filename;

    /**
     * @brief Дескриптор исходного файла
     */
    int fd;

    /**
     * @brief Временный файл, в который необходимо сохранять отсортированные значения
     */
    temp_file_t *temp_file;
} sort_element_t;

static void
sort_element_init(sort_element_t *e, const char *filename)
{
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {

        perror("open");
        exit(1);
    }

    e->temp_file = temp_file_new();
    e->filename = strdup(filename);
    e->fd = fd;
}

static void
sort_element_free(sort_element_t *e)
{
    close(e->fd);
    temp_file_free(e->temp_file);
    free(e->filename);
    e->fd = -1;
    e->filename = NULL;
    e->temp_file = NULL;
}

static void
sort_context_init(coro_sort_context_t *ctx, int id, stack_t *files)
{

    ctx->coroutine_id = id;
    ctx->files = files;
}

/// @brief Функция для запуска алгоритма внешней сортировки файла
/// @param context Указатель на sort_context
/// @return 0 - сортировка произошла успешно, -1 - произошла ошибка
static int
sort_external_coro(void *context)
{
    struct coro *this = coro_this();
    (void)this;
    coro_sort_context_t *ctx = (coro_sort_context_t *)context;

    void *value;
    while (stack_try_pop(ctx->files, &value))
    {
        sort_element_t *se = (sort_element_t *)value;
        sort_file_external_coro(se->fd, temp_file_fd(se->temp_file));
    }

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
    fprintf(stderr, "Количество корутин: %d\n", args->coro_count);
    coro_sched_init(&coro_lat);
}

static void display_coro_stats(struct coro *c)
{
    coro_stats_t stats;
    coro_stats(c, &stats);
    printf("Корутина завершилась:\n\tВремя работы: %lld с, %lld нс\n\tПереключений контекста: %lld\n\tЛожных переключений контекста: %lld\n", 
    (long long)stats.worktime.tv_sec, (long long)stats.worktime.tv_nsec, 
    (long long)stats.switch_count, (long long)stats.false_switch_count);
}

static sort_element_t *
create_sort_elements(const char **filenames, int count)
{
    sort_element_t *sort_elements = (sort_element_t *) malloc(sizeof(sort_element_t) * count);
    for (int i = 0; i < count; i++)
    {
        sort_element_init(sort_elements + i, filenames[i]);
    }
    return sort_elements;
}

static void 
init_files_stack(stack_t *files_stack, sort_element_t* elements, int count)
{
    stack_init(files_stack);
    for (int i = 0; i < count; i++)
    {
        stack_push(files_stack, elements + i);
    }
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
    sort_element_t *sort_elements = create_sort_elements(args.filenames, args.files_count);
    stack_t files_stack;
    init_files_stack(&files_stack, sort_elements, args.files_count);

    coro_sort_context_t *contexts = (coro_sort_context_t *) malloc(sizeof(coro_sort_context_t) * args.coro_count);
    for (long i = 0; i < args.coro_count; i++)
    {
        coro_sort_context_t *cur_ctx = contexts + i;
        sort_context_init(cur_ctx, i, &files_stack);
        coro_new(sort_external_coro, cur_ctx);
    }

    struct timespec start_time;
    clock_gettime(CLOCK_REALTIME, &start_time);

    struct coro *c;
    while ((c = coro_sched_wait()) != NULL)
    {
        display_coro_stats(c);
        coro_delete(c);
    }

    int *fds = (int *)malloc(sizeof(int) * args.files_count);
    for (long i = 0; i < args.files_count; i++)
    {
        int temp_fd = temp_file_fd(sort_elements[i].temp_file);
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
    for (int i = 0; i < args.files_count; i++)
    {
        sort_element_free(sort_elements + i);
    }

    free(fds);
    free(sort_elements);
    free(contexts);
    free(args.filenames);
    stack_free(&files_stack);
    return 0;
}
