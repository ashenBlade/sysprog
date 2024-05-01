#include <stddef.h>
#include <stdlib.h>

#include "recv_buf.h"

void
recv_buf_init(struct recv_buf *buf, int len)
{
    buf->buf = calloc(len, sizeof(char));
    buf->len = len;
    buf->pos = 0;
}

int
recv_buf_left(struct recv_buf *buf)
{
    return buf->len - buf->pos;
}

void
recv_buf_get_pending_buf(struct recv_buf *buf, char **out_buf, int *len)
{
    *out_buf = buf->buf + buf->pos;
    *len = buf->len - buf->pos;
}

void
recv_buf_record_sent(struct recv_buf *buf, int received)
{
    buf->pos += received;
}

bool
recv_buf_ready(struct recv_buf *buf)
{
    return buf->len <= buf->pos;
}

void
recv_buf_free(struct recv_buf *buf)
{
    /* Не вызываю free на буфер, т.к. эта строка будет использоваться в chat_message */
    buf->buf = NULL;
    buf->len = 0;
    buf->pos = 0;
}

bool recv_buf_is_init(struct recv_buf *buf)
{
    return buf->buf != NULL;
}