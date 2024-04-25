#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <stdatomic.h>
#include <assert.h>

#include "task_queue.h"

typedef struct list_entry
{
    void *data;
    struct list_entry *next;
} entry_t;

typedef struct queue
{
    entry_t *head;
    entry_t *tail;
    /* Изначально хотел сделать конкурентную очередь Майкла Скотта,
     * но по непонятным причинам не смог корректно реализовать.
     * Использовал CAS из stdatomic (atomic_compare_exchange_strong),
     * но не сработало и при конкурентной работе возникают ошибки.
     * Причина не ясна, но лучше хоть как-то (на блокировках), чем никак.
     */
    pthread_mutex_t op_mutex;
    int size;
} queue_t;

static void queue_init(queue_t *queue)
{
    entry_t *dummy = (entry_t *)malloc(sizeof(entry_t));
    dummy->data = NULL;
    dummy->next = NULL;

    queue->head = dummy;
    queue->tail = dummy;
    queue->size = 0;

    pthread_mutex_init(&queue->op_mutex, NULL);
}

static void queue_destroy(queue_t *queue)
{
    /* При удалении не заботимся о volatile */
    entry_t *entry = (entry_t *)queue->head;
    while (entry != NULL)
    {
        entry_t *next = (entry_t *)entry->next;
        free(entry);
        entry = next;
    }

    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    pthread_mutex_destroy(&queue->op_mutex);
}

static entry_t *queue_dequeue(queue_t *queue)
{
    entry_t *result = NULL;
    pthread_mutex_lock(&queue->op_mutex);

    if (0 < queue->size)
    {
        result = queue->head->next;
        queue->head = result;
        --queue->size;
    }

    pthread_mutex_unlock(&queue->op_mutex);
    return result;
}

static void queue_enqueue(queue_t *queue, void *data)
{
    entry_t *new_tail = (entry_t *)calloc(1, sizeof(entry_t));
    new_tail->next = NULL;
    new_tail->data = data;

    pthread_mutex_lock(&queue->op_mutex);

    queue->tail->next = new_tail;
    queue->tail = new_tail;
    ++queue->size;

    pthread_mutex_unlock(&queue->op_mutex);
}

struct task_queue
{
    queue_t queue;
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    volatile bool running;
};

task_queue_t *
task_queue_new()
{
    task_queue_t *queue = (task_queue_t *)calloc(1, sizeof(task_queue_t));
    pthread_cond_init(&queue->cond, NULL);
    pthread_mutex_init(&queue->mutex, NULL);
    queue_init(&queue->queue);
    queue->running = true;
    return queue;
}

void task_queue_destroy(task_queue_t *queue)
{
    atomic_store_explicit(&queue->running, false, __ATOMIC_RELEASE);

    /*
     * Говорим всем, что необходимо завершить работу.
     * Но каким-то потокам сигнал мог не дойти и они все еще находятся в состоянии ожидания.
     * В этом случае, destroy вернет EBUSY - просто еще раз запускаем операцию
     */
    pthread_mutex_lock(&queue->mutex);
    int ret_code = EBUSY;
    while (ret_code == EBUSY)
    {
        pthread_cond_broadcast(&queue->cond);
        ret_code = pthread_cond_destroy(&queue->cond);
    }
    pthread_mutex_unlock(&queue->mutex);

    if (ret_code != 0)
    {
        perror("pthread_cond_destroy");
        exit(1);
    }

    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);

    queue_destroy(&queue->queue);
}

void *task_queue_dequeue(task_queue_t *queue, bool *success)
{
    if (!queue->running)
    {
        *success = false;
        return NULL;
    }

    /* Пытаемся прочитать значение из существующей очереди без блокировок */
    entry_t *popped = queue_dequeue(&queue->queue);
    if (popped != NULL)
    {
        void *data = popped->data;
        // free(popped);
        *success = true;
        return data;
    }

    /* Очередь пуста - блокируемся в ожидании нового элемента */
    int ret_code;
    ret_code = pthread_mutex_lock(&queue->mutex);
    if (ret_code != 0)
    {
        /* Скорее всего mutex был уничтожен из вызова task_queue_destroy */
        if (ret_code == EINVAL || (ret_code == -1 && errno == EINVAL))
        {
            goto fail;
        }

        perror("pthread_mutex_lock");
        exit(1);
    }

    if (!queue->running)
    {
        goto fail;
    }

    do
    {
        ret_code = pthread_cond_wait(&queue->cond, &queue->mutex);

        if (ret_code != 0)
        {
            perror("pthread_cond_wait");
            exit(1);
        }

        if (!queue->running)
        {
            goto fail;
        }

        popped = queue_dequeue(&queue->queue);
    } while (popped == NULL);

    pthread_mutex_unlock(&queue->mutex);

    void *data = popped->data;
    *success = true;
    // free(popped);
    return data;

fail:
    pthread_mutex_unlock(&queue->mutex);
    *success = false;
    return NULL;
}

void task_queue_enqueue(task_queue_t *queue, void *task)
{
    if (!queue->running)
    {
        return;
    }

    queue_enqueue(&queue->queue, task);

    pthread_mutex_lock(&queue->mutex);
    if (pthread_cond_signal(&queue->cond))
    {
        perror("pthread_cond_signal");
        exit(1);
    }
    pthread_mutex_unlock(&queue->mutex);
}

int task_queue_size(task_queue_t *queue)
{
    return queue->queue.size;
}
