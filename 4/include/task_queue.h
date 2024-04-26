#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include <stdbool.h>

typedef struct task_queue task_queue_t;

task_queue_t *task_queue_new();
void task_queue_destroy(task_queue_t *queue);
void task_queue_enqueue(task_queue_t *queue, void *task);
int task_queue_size(task_queue_t *queue);
void *task_queue_dequeue(task_queue_t *queue, bool *success);

#endif