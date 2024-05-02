#ifndef RECV_BUF_H
#define RECV_BUF_H

#include <stdbool.h>

struct recv_buf
{
    char *buf;
    int len;
    int pos;
};

void recv_buf_init(struct recv_buf *buf, int len);
int recv_buf_left(struct recv_buf *buf);
void recv_buf_get_pending_buf(struct recv_buf *buf, char **out_buf, int *len);
void recv_buf_record_sent(struct recv_buf *buf, int received);
bool recv_buf_ready(struct recv_buf *buf);
void recv_buf_free(struct recv_buf *buf);
bool recv_buf_is_init(struct recv_buf *buf);

#endif