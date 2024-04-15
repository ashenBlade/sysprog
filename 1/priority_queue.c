#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "priority_queue.h"
#define DEFAULT_HEAP_CAPACITY 16

#define PARENT(x) (((x)-1) / 2)
#define LEFT_CHILD(x) (2 * (x) + 1)
#define RIGHT_CHILD(x) (2 * (x) + 2)

void priority_queue_init(priority_queue_t *pq)
{
    pq->size = 0;
    pq->capacity = 0;
    pq->heap = NULL;
}

void priority_queue_delete(priority_queue_t *pq)
{
    free(pq->heap);
    pq->heap = NULL;
    pq->size = 0;
    pq->capacity = 0;
}

static inline void swap(pq_entry_t *heap, int left, int right)
{
    pq_entry_t tmp = heap[left];
    heap[left] = heap[right];
    heap[right] = tmp;
}

static void heapify_up(priority_queue_t *pq)
{
    /* Наш новый элемент находится последним */
    int current_index = pq->size - 1;
    while (0 < current_index)
    {
        int parent_index = PARENT(current_index);
        if (pq->heap[current_index].key < pq->heap[parent_index].key)
        {
            swap(pq->heap, parent_index, current_index);
            current_index = parent_index;
        }
        else
        {
            break;
        }
    }
}

void priority_queue_enqueue(priority_queue_t *pq, int key, void *value)
{
    assert(pq != NULL);

    if (pq->capacity == 0)
    {
        pq_entry_t *heap = (pq_entry_t *)malloc(sizeof(pq_entry_t) * DEFAULT_HEAP_CAPACITY);
        pq->size = 1;
        pq->capacity = DEFAULT_HEAP_CAPACITY;
        heap[0] = (pq_entry_t){
            .key = key,
            .value = value};
        pq->heap = heap;
        return;
    }

    if (pq->size == pq->capacity)
    {
        int new_capacity = pq->capacity * 2;
        pq->heap = (pq_entry_t *)realloc(pq->heap, new_capacity);
        pq->capacity = new_capacity;
    }

    pq->heap[pq->size] = (pq_entry_t){
        .key = key,
        .value = value};
    ++pq->size;
    heapify_up(pq);
}

static void heapify_down(priority_queue_t *pq)
{
    int current_index = 0;
    while (LEFT_CHILD(current_index) < pq->size)
    {
        int left = LEFT_CHILD(current_index);
        int right = RIGHT_CHILD(current_index);

        if (right < pq->size)
        {
            /* Есть оба потомка */
            int min = pq->heap[left].key < pq->heap[right].key
                          ? left
                          : right;
            if (pq->heap[current_index].key < pq->heap[min].key)
            {
                return;
            }
            swap(pq->heap, min, current_index);
            current_index = min;
        }
        else
        {
            /* 
             * Если есть только левый потомок, то проверяем только его и заканчиваем,
             * т.к. это означает конец кучи (массива)
             */
            if (pq->heap[left].key < pq->heap[current_index].key)
            {
                return;
            }

            swap(pq->heap, left, current_index);
            return;
        }
    }
}

bool priority_queue_try_dequeue(priority_queue_t *pq, int *key, void **value)
{
    if (pq->size == 0)
    {
        return false;
    }

    pq_entry_t top_entry = pq->heap[0];
    *key = top_entry.key;
    *value = top_entry.value;

    if (pq->size == 1)
    {
        pq->size = 0;
        memset(pq->heap, 0, sizeof(pq_entry_t));
        return true;
    }

    swap(pq->heap, 0, pq->size - 1);
    pq->size -= 1;
    heapify_down(pq);
    return true;
}
