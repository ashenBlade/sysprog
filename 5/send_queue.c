#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <arpa/inet.h>

#include "send_queue.h"


void 
send_queue_init(struct send_queue *queue)
{
    queue->head = NULL;
    queue->tail = NULL;
}

void send_queue_free(struct send_queue *q)
{
    if (q->head == NULL)
    {
        return;
    }

    for (struct send_queue_elem * cur = q->head; cur != NULL; cur = cur->next)
    {
        free(cur->data);
        cur->data = NULL;
        cur->len = 0;
        cur->pos = 0;
    }

    q->head = NULL;
    q->tail = NULL;
}

void
send_queue_enqueue(struct send_queue *queue, char *data, int len)
{
    struct send_queue_elem *new_elem = calloc(1, sizeof(struct send_queue_elem));
    new_elem->next = NULL;
    new_elem->data = data;
    new_elem->len = len;
    new_elem->pos = 0;

    if (queue->head == NULL)
    {
        queue->head = new_elem;
        queue->tail = new_elem;
    }
    else
    {
        queue->tail->next = new_elem;
        queue->tail = new_elem;
    }
}

void
send_queue_enqueue_len(struct send_queue *queue, char *data, int len)
{
    char *send_str = calloc(len + sizeof(int), sizeof(char));
    memcpy(send_str + sizeof(int), data, len);
    int size = htonl(len);
    memcpy(send_str, (char *)&size, sizeof(int));
    send_queue_enqueue(queue, send_str, len + sizeof(int));
}

int
send_queue_get_pending_chunk(struct send_queue *queue, char **data, int *len)
{
    if (queue->head == NULL)
    {
        return -1;
    }
    struct send_queue_elem *head = queue->head;
    assert(head->pos < head->len);
    *data = head->data + head->pos;
    *len = head->len - head->pos;
    return 0;
}

void
send_queue_record_sent(struct send_queue *queue, int send)
{
    assert(queue->head != NULL);

    struct send_queue_elem *head = queue->head;
    head->pos += send;
    if (head->pos < head->len)
    {
        return;
    }

    if (head->next == NULL)
    {
        queue->head = NULL;
        queue->tail = NULL;
    }
    else
    {
        queue->head = head->next;
    }
    head->next = NULL;
    head->pos = 0;
    head->len = 0;
    free(head->data);
    free(head);
}

bool
send_queue_empty(const struct send_queue *queue)
{
    /* Данные есть, пока очередь не пуста */
    return queue->head == NULL;
}
