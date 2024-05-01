#ifndef QUEUE_H
#define QUEUE_H

struct queue_elem
{
    struct queue_elem *next;
    void *data;
};

struct queue
{
    struct queue_elem *head;
    struct queue_elem *tail;
};

void queue_init(struct queue *q);
void queue_free(struct queue *q);
int queue_is_empty(const struct queue *q);
void queue_enqueue(struct queue *q, void *data);
int queue_dequeue(struct queue *q, void **data);

#endif