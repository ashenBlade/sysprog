#include <stddef.h>
#include <stdlib.h>

#include "queue.h"

typedef struct queue queue_t;
typedef struct queue_elem elem_t;

void queue_init(queue_t *q)
{
    q->head = NULL;
    q->tail = NULL;
}

void queue_free(queue_t *q)
{
    elem_t *cur = q->head;
    q->head = NULL;
    q->tail = NULL;

    while (cur != NULL)
    {
        elem_t *next = cur->next;
        free(cur);
        cur = next;
    }
}

void queue_enqueue(queue_t *q, void *data)
{
    elem_t *entry = calloc(1, sizeof(elem_t));
    entry->next = NULL;
    entry->data = data;

    if (q->tail == NULL)
    {
        q->tail = entry;
        q->head = entry;
    }
    else
    {
        q->tail->next = entry;
        q->tail = entry;
    }
}

int queue_dequeue(queue_t *q, void **data)
{
    if (q->head == NULL)
    {
        return -1;
    }

    elem_t *head = q->head;
    if (head->next == NULL)
    {
        q->head = NULL;
        q->tail = NULL;
    }
    else
    {
        q->head = head->next;
    }

    *data = head->data;
    head->next = NULL;
    head->data = NULL;
    free(head);
    return 0;
}

int queue_is_empty(const queue_t *q)
{
    return q->head == NULL;
}
