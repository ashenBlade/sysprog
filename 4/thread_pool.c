#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <stdatomic.h>

#include "task_queue.h"
#include "thread_pool.h"

enum task_state
{
    /* Создана - просто "вызван конструктор" */
    TASK_STATE_CREATED = 0,
    /* В очереди и ожидает начала обработки */
    TASK_STATE_PENDING = 1,
    /* В процессе работы */
    TASK_STATE_RUNNING = 2,
    /* Завершена */
    TASK_STATE_FINISHED = 3,
    /* Для задачи вызван JOIN и результат получен */
    TASK_STATE_JOINED = 4,
    /* Удалена - "вызван деструктор" */
    TASK_STATE_DESTROYED = -1,
};

#define TASK_STATE_IS_TERMINAL(state) ((state) == TASK_STATE_FINISHED || (state) == TASK_STATE_JOINED || (state) == TASK_STATE_DESTROYED)

typedef struct thread_task
{
    thread_task_f function;
    void *arg;
    volatile void *ret_val;
    volatile enum task_state state;
    pthread_cond_t finished_cond;
    pthread_mutex_t finished_lock;
} ttask_t;

static ttask_t *task_new(thread_task_f function, void *arg)
{
    ttask_t *tt = (ttask_t *)calloc(1, sizeof(ttask_t));
    tt->function = function;
    tt->arg = arg;
    tt->ret_val = NULL;
    tt->state = TASK_STATE_CREATED;
    pthread_cond_init(&tt->finished_cond, NULL);
    pthread_mutex_init(&tt->finished_lock, NULL);
    return tt;
}

static void task_destroy(ttask_t *tt)
{
    pthread_cond_destroy(&tt->finished_cond);
    pthread_mutex_destroy(&tt->finished_lock);
    tt->state = TASK_STATE_DESTROYED;
    tt->function = NULL;
    tt->arg = NULL;
    tt->ret_val = NULL;
    free(tt);
}

static void task_set_result(ttask_t *tt, void *result)
{
    assert(tt->state == TASK_STATE_RUNNING);

    if (result == NULL)
    {
        write(STDOUT_FILENO, "null\n", 5);
    }

    atomic_store_explicit(&tt->ret_val, result, __ATOMIC_RELEASE);
    atomic_store_explicit(&tt->state, TASK_STATE_FINISHED, __ATOMIC_RELEASE);

    pthread_mutex_lock(&tt->finished_lock);
    pthread_cond_broadcast(&tt->finished_cond);
    pthread_mutex_unlock(&tt->finished_lock);
}

static void *task_wait_result(ttask_t *tt)
{
    if (TASK_STATE_IS_TERMINAL(tt->state))
    {
        return (void*) tt->ret_val;
    }

    pthread_mutex_lock(&tt->finished_lock);
    while (!TASK_STATE_IS_TERMINAL(tt->state))
    {
        pthread_cond_wait(&tt->finished_cond, &tt->finished_lock);
    }
    pthread_mutex_unlock(&tt->finished_lock);

    return (void*) tt->ret_val;
}

typedef struct thread_pool
{
    /**
     * @brief Потоки этого пула
     */
    pthread_t *threads;
    /**
     * @brief Количество работающих потоков
     */
    int count;
    /**
     * @brief Максимальный размер пула
     */
    int capacity;
    /**
     * @brief Блокировка для обновления списка потоков
     */
    pthread_mutex_t update_lock;

    /**
     * @brief Задачи, ожидающие выполнения
     */
    task_queue_t *pending;

    /**
     * @brief Количество работающих потоков в данный момент
     */
    int busy;
    /**
     * @brief Работает ли пул в данный момент
     */
    bool running;
} tpool_t;

static tpool_t *tpool_new(int capacity)
{
    tpool_t *p = (tpool_t *)calloc(1, sizeof(tpool_t));
    p->threads = (pthread_t *)calloc(capacity, sizeof(pthread_t));
    pthread_mutex_init(&p->update_lock, NULL);
    p->count = 0;
    p->capacity = capacity;
    p->pending = task_queue_new();
    p->running = true;
    p->busy = 0;
    return p;
}

static void tpool_delete(tpool_t *pool)
{
    pool->running = false;
    task_queue_destroy(pool->pending);
    for (int i = 0; i < pool->count; i++)
    {
        pthread_join(pool->threads[i], NULL);
    }
    free(pool->threads);
    pool->threads = NULL;
    pool->capacity = 0;
    pool->count = 0;
    pool->busy = 0;
}

static int tpool_total_tasks(tpool_t *p)
{
    return task_queue_size(p->pending) + p->busy;
}

static bool tpool_is_highload(tpool_t *pool)
{
    return pool->busy == pool->count;
}

static void *
thread_worker(void *data)
{
    tpool_t *pool = (tpool_t *)data;
    while (true)
    {
        bool success;
        ttask_t *tt = (ttask_t *)task_queue_dequeue(pool->pending, &success);
        if (!success)
        {
            /* Необходимо завершить работу */
            pthread_exit(NULL);
        }
        if (tt == NULL)
        {
            continue;
        }

        if (tt->state != TASK_STATE_PENDING)
        {
            continue;
        }

        tt->state = TASK_STATE_RUNNING;
        atomic_fetch_add_explicit(&pool->busy, 1, __ATOMIC_RELEASE);

        void *result = tt->function(tt->arg);
        task_set_result(tt, result);

        atomic_fetch_sub_explicit(&pool->busy, 1, __ATOMIC_RELEASE);
    }
}

static void tpool_run_thread(tpool_t *pool, int saved_count)
{
    /*
     * Чтобы избежать гонки создания новых потоков
     */
    if (saved_count < pool->count)
    {
        return;
    }

    pthread_mutex_lock(&pool->update_lock);

    if (saved_count < pool->count)
    {
        pthread_mutex_unlock(&pool->update_lock);
        return;
    }

    pthread_create(pool->threads + saved_count, NULL, thread_worker, pool);

    ++pool->count;
    pthread_mutex_unlock(&pool->update_lock);
}

static void tpool_enqueue(tpool_t *pool, ttask_t *task)
{
    if (tpool_is_highload(pool) &&
        pool->count < pool->capacity)
    {
        tpool_run_thread(pool, pool->count);
    }

    atomic_store_explicit(&task->state, TASK_STATE_PENDING, __ATOMIC_RELEASE);
    task_queue_enqueue(pool->pending, (void *)task);
}

int thread_pool_new(int max_thread_count, struct thread_pool **pool)
{
    if (max_thread_count < 1 || TPOOL_MAX_THREADS < max_thread_count)
    {
        return TPOOL_ERR_INVALID_ARGUMENT;
    }

    *pool = tpool_new(max_thread_count);
    return 0;
}

int thread_pool_thread_count(const struct thread_pool *pool)
{
    return pool->count;
}

int thread_pool_delete(struct thread_pool *pool)
{
    if (0 < task_queue_size(pool->pending) || 0 < pool->busy)
    {
        return TPOOL_ERR_HAS_TASKS;
    }

    tpool_delete(pool);
    free(pool);
    return 0;
}

int thread_pool_push_task(struct thread_pool *pool, struct thread_task *task)
{
    if (TPOOL_MAX_TASKS <= tpool_total_tasks(pool))
    {
        return TPOOL_ERR_TOO_MANY_TASKS;
    }

    if (task->state == TASK_STATE_CREATED)
    {
        tpool_enqueue(pool, task);
    }

    return 0;
}

int thread_task_new(struct thread_task **task, thread_task_f function, void *arg)
{
    *task = task_new(function, arg);
    return 0;
}

bool thread_task_is_finished(const struct thread_task *task)
{
    return task->state == TASK_STATE_FINISHED;
}

bool thread_task_is_running(const struct thread_task *task)
{
    return task->state == TASK_STATE_RUNNING;
}

int thread_task_join(struct thread_task *task, void **result)
{
    switch (task->state)
    {
    case TASK_STATE_CREATED:
        return TPOOL_ERR_TASK_NOT_PUSHED;
    case TASK_STATE_FINISHED:
        task->state = TASK_STATE_JOINED;
        /* fall-through */
    case TASK_STATE_JOINED:
        *result = (void*)task->ret_val;
        return 0;
    case TASK_STATE_DESTROYED:
        return TPOOL_ERR_INVALID_ARGUMENT;
    case TASK_STATE_PENDING:
    case TASK_STATE_RUNNING:
        break;
    }

    pthread_mutex_lock(&task->finished_lock);
    while (task->state == TASK_STATE_RUNNING || task->state == TASK_STATE_PENDING)
    {
        pthread_cond_wait(&task->finished_cond, &task->finished_lock);
    }
    pthread_mutex_unlock(&task->finished_lock);

    *result = (void*) task->ret_val;
    task->state = TASK_STATE_JOINED;
    return 0;
}

#ifdef NEED_TIMED_JOIN

int thread_task_timed_join(struct thread_task *task, double timeout, void **result)
{
    /* IMPLEMENT THIS FUNCTION */
    (void)task;
    (void)timeout;
    (void)result;
    return TPOOL_ERR_NOT_IMPLEMENTED;
}

#endif

int thread_task_delete(struct thread_task *task)
{
    if (task->state == TASK_STATE_DESTROYED)
    {
        return 0;
    }

    if (task->state == TASK_STATE_CREATED || task->state == TASK_STATE_JOINED)
    {
        task_destroy(task);
        return 0;
    }

    return TPOOL_ERR_TASK_IN_POOL;
}

#ifdef NEED_DETACH

int thread_task_detach(struct thread_task *task)
{
    /* IMPLEMENT THIS FUNCTION */
    (void)task;
    return TPOOL_ERR_NOT_IMPLEMENTED;
}

#endif
