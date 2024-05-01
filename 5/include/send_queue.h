#ifndef SEND_QUEUE_H
#define SEND_QUEUE_H

#include <stdbool.h>

struct send_queue_elem
{
    struct send_queue_elem *next;
    char *data;
    int len;
    int pos;
};

struct send_queue
{
    struct send_queue_elem *head;
    struct send_queue_elem *tail;
};

void send_queue_init(struct send_queue *queue);
void send_queue_free(struct send_queue *queue);
void send_queue_enqueue(struct send_queue *queue, char *data, int len);
/* Добавить в очередь отправки строку, но в данных добавить префикс длины сообщения */
void send_queue_enqueue_len(struct send_queue *queue, char *data, int data_len);
int send_queue_get_pending_chunk(struct send_queue *queue, char **data, int *len);
void send_queue_record_sent(struct send_queue *queue, int send);
bool send_queue_empty(const struct send_queue *queue);


#endif