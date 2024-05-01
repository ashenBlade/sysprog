#include <assert.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <poll.h>
#include <netdb.h>

#include "chat.h"
#include "chat_client.h"
#include "recv_buf.h"
#include "send_queue.h"

struct input_buf
{
    /* Временный буфер для несформированных сообщений */
    char *tmp_buf;
    int tmp_buf_len;
};

static int
generate_msg_str(const char *data, int start, int end, char **out_str, int *out_len)
{
    /*
     * Находим настоящее начало и концы (убираем пробелы).
     * Если пусто - -1
     */

    while (isspace(data[start]) && start < end)
    {
        ++start;
    }

    if (start == end)
    {
        return -1;
    }

    while (isspace(data[end]) && start < end && isspace(data[end - 1]))
    {
        --end;
    }

    if (start == end)
    {
        return -1;
    }

    int len = end - start;
    char *buf = calloc(len, sizeof(char));
    memcpy(buf, &data[start], len);
    *out_str = buf;
    *out_len = len;
    return 0;
}

struct chat_client
{
    /** Socket connected to the server. */
    int socket;
    struct pollfd pfd;
    /** Array of received messages. */
    struct chat_message **received;
    int received_cnt;
    int received_cap;

    struct input_buf out_buf;
    struct recv_buf recv_buf;
    struct send_queue send_queue;
};

#define CLIENT_IS_INIT(client) ((client)->socket != -1)

static void
process_user_input(struct chat_client *client, const char *data, int len)
{
    const char *str_tmp_buf = data;
    int str_tmp_len = len;
    struct input_buf *buf = &client->out_buf;
    struct send_queue *send_queue = &client->send_queue;
    if (buf->tmp_buf_len != 0)
    {
        /* Не самый оптимальный вариант - конкатенировать строки, но так легче */
        char *tmp = calloc(len + buf->tmp_buf_len, sizeof(char));
        memcpy(tmp, buf->tmp_buf, buf->tmp_buf_len);
        memcpy(tmp + buf->tmp_buf_len, data, len);

        str_tmp_len = len + buf->tmp_buf_len;
        str_tmp_buf = tmp;
    }

    int start = 0;
    int end = 0;

    while (end < str_tmp_len)
    {
        /* Ищем символ новой строки */
        while (str_tmp_buf[end] != '\n' && end < str_tmp_len)
        {
            ++end;
        }

        /* Если не нашли и дошли до конца, то создаем новый временный буфер */
        if (end == str_tmp_len)
        {
            /* Создаем новый временный буфер - конец */
            int new_tmp_buf_len = end - start;
            char *new_tmp_buf = calloc(new_tmp_buf_len, sizeof(char));
            memcpy(new_tmp_buf, str_tmp_buf + start, new_tmp_buf_len);
            buf->tmp_buf = new_tmp_buf;
            buf->tmp_buf_len = new_tmp_buf_len;
            break;
        }

        char *str;
        int len;
        if (generate_msg_str(str_tmp_buf, start, end, &str, &len) == 0)
        {
            char *send_str = calloc(len + sizeof(int), sizeof(char));
            memcpy(send_str + 4, str, len);
            int size = htonl(len);
            memcpy(send_str, (char *)&size, sizeof(int));
            send_queue_enqueue(send_queue, send_str, len + sizeof(int));
            client->pfd.events |= POLLOUT;
            free(str);
        }

        while (isspace(data[end]) && end < str_tmp_len)
        {
            ++end;
        }

        start = end;
    }

    if (str_tmp_buf != data)
    {
        free((char *)str_tmp_buf);
    }
}

static void
chat_client_add_message(struct chat_client *client, struct chat_message *message)
{
    if (client->received_cap == 0)
    {
        client->received = calloc(1, sizeof(struct chat_message *));
        client->received_cap = 1;
    }
    else if (client->received_cap == client->received_cnt)
    {
        client->received_cap *= 2;
        client->received = realloc(client->received, client->received_cap * sizeof(struct chat_message *));
    }

    client->received[client->received_cnt] = message;
    ++client->received_cnt;
}

struct chat_client *
chat_client_new(const char *name)
{
    (void)name;

    struct chat_client *client = calloc(1, sizeof(*client));
    memset(client, 0, sizeof(*client));

    client->socket = -1;

    send_queue_init(&client->send_queue);

    client->received = NULL;
    client->received_cnt = 0;
    client->received_cap = 0;

    return client;
}

void chat_client_delete(struct chat_client *client)
{
    if (client->socket >= 0)
        close(client->socket);

    send_queue_free(&client->send_queue);
    recv_buf_free(&client->recv_buf);

    free(client);
}

static int
split_addr(const char *addr, char *ip, char *port)
{
    int index = 0;
    char ch;
    while ((ch = addr[index]) != '\0')
    {
        if (ch == ':')
        {
            break;
        }

        ++index;
    }

    if (ch == '\0')
    {
        return -1;
    }

    int i = 0;
    for (; i < index; i++)
    {
        ip[i] = addr[i];
    }
    ip[index] = '\0';

    ++i;
    int j = 0;
    while ((ch = addr[i]) != '\0')
    {
        port[j] = ch;
        ++j;
        ++i;
    }
    port[j] = '\0';

    return 0;
}

int chat_client_connect(struct chat_client *client, const char *addr)
{
    (void)client;
    (void)addr;
    char ip_addr[16];
    char port_addr[8];
    memset(ip_addr, 0, sizeof(ip_addr));
    if (split_addr(addr, ip_addr, port_addr) == -1)
    {
        return CHAT_ERR_INVALID_ARGUMENT;
    }

    struct addrinfo *res;
    memset(&res, 0, sizeof(res));
    struct addrinfo hint;
    memset(&hint, 0, sizeof(hint));
    hint.ai_family = AF_INET;
    hint.ai_protocol = 0;
    hint.ai_socktype = SOCK_STREAM;

    int rc;
    if ((rc = getaddrinfo(ip_addr, port_addr, &hint, &res)) != 0)
    {
        dprintf(STDERR_FILENO, "getaddrinfo: %s\n", gai_strerror(rc));
        return CHAT_ERR_SYS;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    int csock = -1;
    for (struct addrinfo *cur = res; cur != NULL; cur = cur->ai_next)
    {
        int sock = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
        if (sock == -1)
        {
            continue;
        }

        if (connect(sock, cur->ai_addr, cur->ai_addrlen) == 0)
        {
            csock = sock;
            break;
        }

        close(sock);
    }

    freeaddrinfo(res);

    if (csock == -1)
    {
        return CHAT_ERR_NO_ADDR;
    }

    client->socket = csock;
    client->pfd.fd = csock;
    client->pfd.events = POLLIN;

    return 0;
}

struct chat_message *
chat_client_pop_next(struct chat_client *client)
{

    (void)client;
    return NULL;
}

static int get_timeout(double timeout)
{
    if (timeout == -1)
    {
        return -1;
    }

    return timeout * 1000;
}

static int recv_int(int sock)
{
    char buf[4];
    int cur = 0;
    while (cur < 4)
    {
        int r = recv(sock, buf + cur, sizeof(buf) - cur, 0);
        if (r == -1)
        {
            return -1;
        }

        cur += r;
    }

    return ntohl(*((int *)buf));
}

static int
chat_client_receive_message(struct chat_client *client)
{
    if (!recv_buf_is_init(&client->recv_buf))
    {
        int len = recv_int(client->socket);
        if (len == -1)
        {
            return -1;
        }
        recv_buf_init(&client->recv_buf, len);
    }

    char *buf;
    int len;
    recv_buf_get_pending_buf(&client->recv_buf, &buf, &len);
    int read = recv(client->socket, buf, len, 0);
    if (read == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 0;
        }

        return -1;
    }

    recv_buf_record_sent(&client->recv_buf, read);
    if (!recv_buf_ready(&client->recv_buf))
    {
        return 0;
    }

    struct chat_message *msg = calloc(1, sizeof(struct chat_message));
    msg->data = client->recv_buf.buf;
    msg->len = client->recv_buf.len;
    chat_client_add_message(client, msg);
    recv_buf_free(&client->recv_buf);
    return 0;
}

static int
chat_client_send_message(struct chat_client *client)
{
    char *buf;
    int len;
    if (send_queue_get_pending_chunk(&client->send_queue, &buf, &len) == -1)
    {
        client->pfd.events &= ~POLLOUT;
        return 0;
    }

    int sent = send(client->socket, buf, len, 0);
    if (sent == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 0;
        }
        return -1;
    }

    send_queue_record_sent(&client->send_queue, sent);
    if (send_queue_empty(&client->send_queue))
    {
        client->pfd.events &= ~POLLOUT;
    }

    return 0;
}

int chat_client_update(struct chat_client *client, double timeout)
{
    if (!CLIENT_IS_INIT(client))
    {
        return CHAT_ERR_NOT_STARTED;
    }

    int rc = poll(&client->pfd, 1, get_timeout(timeout));
    if (rc == -1)
    {
        return CHAT_ERR_SYS;
    }

    if (rc == 0)
    {
        return CHAT_ERR_TIMEOUT;
    }


    if (client->pfd.revents & POLLIN)
    {
        if (chat_client_receive_message(client) == -1)
        {
            return CHAT_ERR_SYS;
        }
    }

    if (client->pfd.revents & POLLOUT)
    {
        if (chat_client_send_message(client) == -1)
        {
            return CHAT_ERR_SYS;
        }
    }

    return 0;
}

int chat_client_get_descriptor(const struct chat_client *client)
{
    return client->socket;
}

int chat_client_get_events(const struct chat_client *client)
{
    if (!CLIENT_IS_INIT(client))
    {
        return 0;
    }

    if (send_queue_empty(&client->send_queue))
    {
        return CHAT_EVENT_INPUT;
    }

    return CHAT_EVENT_INPUT | CHAT_EVENT_OUTPUT;
}

int chat_client_feed(struct chat_client *client, const char *msg, uint32_t msg_size)
{
    process_user_input(client, msg, msg_size);
    if (!send_queue_empty(&client->send_queue))
    {
        client->pfd.events |= POLLOUT;
    }

    return 0;
}
