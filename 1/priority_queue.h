#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include <stdbool.h>

typedef struct priority_queue_entry
{
    int key;
    void *value;
} pq_entry_t;

typedef struct priority_queue
{
    pq_entry_t *heap;
    int size;
    int capacity;
} priority_queue_t;

void priority_queue_init(priority_queue_t *pq);
void priority_queue_delete(priority_queue_t *pq);
void priority_queue_enqueue(priority_queue_t *pq, int key, void *value);
bool priority_queue_try_dequeue(priority_queue_t *pq, int *key, void **value);

#endif