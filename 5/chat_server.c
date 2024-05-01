#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <wait.h>
#include <assert.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <poll.h>

#include "chat.h"
#include "chat_server.h"
#include "recv_buf.h"
#include "send_queue.h"

#define LISTEN_BACKLOG 10

struct chat_peer
{
    /** Client's socket. To read/write messages. */
    int socket;

    /** Output buffer. */
    struct send_queue send_queue;
    struct recv_buf recv_buf;

    /* PUT HERE OTHER MEMBERS */
    const char *username;
};

struct message_peer
{
    /* Сокет клиента, который это сообщение отправил */
    int author;
    struct chat_message *msg;
};

#define CHAT_PEER_DISCONNECTED(peer) ((peer)->socket == SOCKET_NOT_INIT)

static void
chat_peer_init(struct chat_peer *peer, int fd)
{
    memset(peer, 0, sizeof(*peer));
    peer->socket = fd;
    peer->username = NULL;
    peer->send_queue.head = NULL;
    peer->send_queue.tail = NULL;
}

#define SOCKET_NOT_INIT -1

struct chat_server
{
    /** Listening socket. To accept new clients. */
    int socket;
    /** Array of peers. */
    struct chat_peer *peers;

    /* Индекс 0 - pollfd сервера, остальные - клиентские */
    struct pollfd *fds;
    int fds_cnt;
    int fds_cap;

    /* Список пришедших сообщений, ожидающих отправки */
    struct message_peer **msgs;
    int msgs_cnt;
    int msgs_cap;
};

#define SERVER_IS_INIT(server) ((server)->socket != SOCKET_NOT_INIT)
#define SERVER_HAS_ACCEPT(server) (((server)->fds[0].revents & POLLIN) != 0)
#define CLIENT_READY_READ(pollfd) (((pollfd)->revents & POLLIN) != 0)
#define CLIENT_READY_WRITE(pollfd) (((pollfd)->revents & POLLOUT) != 0)

static void
socket_make_nonblock(int sock)
{
    int flags = fcntl(sock, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(sock, F_SETFL, flags);
}

struct chat_server *
chat_server_new(void)
{
    struct chat_server *server = calloc(1, sizeof(*server));
    server->socket = SOCKET_NOT_INIT;

    server->fds_cap = 0;
    server->fds_cnt = 0;
    server->fds = NULL;
    server->peers = NULL;

    server->msgs = NULL;
    server->msgs_cnt = 0;
    server->msgs_cap = 0;

    return server;
}

void chat_server_delete(struct chat_server *server)
{
    if (SERVER_IS_INIT(server))
        close(server->socket);

    for (int i = 0; i < server->fds_cnt - 1; i++)
    {
        close(server->peers[i].socket);
    }

    free(server->peers);
    free(server->fds);
    free(server->msgs);

    memset(server, 0, sizeof(*server));
    free(server);
}

int chat_server_listen(struct chat_server *server, uint16_t port)
{
    if (SERVER_IS_INIT(server))
    {
        return CHAT_ERR_ALREADY_STARTED;
    }

    int ssock = socket(AF_INET, SOCK_STREAM, 0);
    if (ssock == -1)
    {
        return CHAT_ERR_SYS;
    }

    socket_make_nonblock(ssock);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(ssock, (const struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1)
    {
        close(ssock);
        if (errno == EADDRINUSE)
        {
            return CHAT_ERR_PORT_BUSY;
        }

        return CHAT_ERR_SYS;
    }

    if (listen(ssock, LISTEN_BACKLOG) == -1)
    {
        close(ssock);
        return CHAT_ERR_SYS;
    }

    server->peers = NULL;

    server->fds = calloc(1, sizeof(*server->fds));
    server->fds_cnt = 1;
    server->fds_cap = 1;

    server->fds[0].fd = ssock;
    server->fds[0].events = POLLIN;

    server->socket = ssock;
    return 0;
}

static void
chat_server_add_message(struct chat_server *s, struct message_peer *message)
{
    if (s->msgs_cap == 0)
    {
        s->msgs = calloc(1, sizeof(struct message_peer *));
        s->msgs_cnt = 1;
        s->msgs_cap = 1;
        s->msgs[0] = message;
        return;
    }

    if (s->msgs_cap == s->msgs_cnt)
    {
        s->msgs_cap *= 2;
        s->msgs = realloc(s->msgs, s->msgs_cap * sizeof(struct message_peer *));
    }

    s->msgs[s->msgs_cnt] = message;
    ++s->msgs_cnt;
}

static int
chat_server_pop_message(struct chat_server *server, struct message_peer **msg)
{
    if (server->msgs_cnt == 0)
    {
        return -1;
    }

    *msg = server->msgs[server->msgs_cnt - 1];
    --server->msgs_cnt;
    return 0;
}

struct chat_message *
chat_server_pop_next(struct chat_server *server)
{
    struct message_peer *msg = NULL;
    if (chat_server_pop_message(server, &msg) == -1)
    {
        return NULL;
    }

    for (int i = 1; i < server->fds_cnt; i++)
    {
        struct pollfd *fd = server->fds + i;
        if (fd->fd == msg->author)
        {
            continue;
        }

        char *send_str = calloc(msg->msg->len + sizeof(int), sizeof(char));
        memcpy(send_str + sizeof(int), msg->msg->data, msg->msg->len);
        int size = htonl(msg->msg->len);
        memcpy(send_str, (char *)&size, sizeof(int));
        send_queue_enqueue(&server->peers[i - 1].send_queue, send_str, msg->msg->len + sizeof(int));

        fd->events |= POLLIN;
    }

    struct chat_message *m = msg->msg;
    free(msg);
    return m;
}

static void
chat_server_ensure_capacity(struct chat_server *s)
{
    assert(SERVER_IS_INIT(s));
    if (s->fds_cap == 1)
    {
        /* Есть только сервер - надо инициализировать исходный  */
        s->peers = calloc(1, sizeof(*s->peers));
        s->fds_cap = 2;
        s->fds = realloc(s->fds, s->fds_cap * sizeof(*s->fds));
    }
    else if (s->fds_cap == s->fds_cnt)
    {
        s->fds_cap *= 2;
        s->fds = realloc(s->fds, s->fds_cap * sizeof(*s->fds));
        s->peers = realloc(s->peers, (s->fds_cap - 1) * sizeof(*s->peers));
    }
}

static struct pollfd *
chat_server_add_client(struct chat_server *s, int cfd)
{
    chat_server_ensure_capacity(s);
    int client_idx = s->fds_cnt;

    struct pollfd *client_pfd = s->fds + client_idx;
    client_pfd->fd = cfd;
    /* Всегда готов принимать сообщения */
    client_pfd->events = POLLIN;

    struct chat_peer *client_peer = s->peers + (client_idx - 1);
    chat_peer_init(client_peer, cfd);
    socket_make_nonblock(cfd);

    ++s->fds_cnt;
    return client_pfd;
}

static int calc_timeout(double timeout_s)
{
    if (timeout_s == -1)
    {
        return -1;
    }

    return timeout_s * 1000;
}

static int
consume_client_message(struct chat_server *server, struct chat_peer *peer)
{
    if (!recv_buf_is_init(&peer->recv_buf))
    {

        char buf[sizeof(int)];
        int len = recv(peer->socket, buf, sizeof(buf), 0);
        if (len == 0)
        {
            return -1;
        }

        if (len == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return 0;
            }

            return -1;
        }

        int str_len = ntohl(*((int *)buf));

        recv_buf_init(&peer->recv_buf, str_len);
    }

    char *buf;
    int len;
    recv_buf_get_pending_buf(&peer->recv_buf, &buf, &len);
    int read = recv(peer->socket, buf, len, 0);
    if (read == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 0;
        }

        return -1;
    }

    recv_buf_record_sent(&peer->recv_buf, read);
    if (!recv_buf_ready(&peer->recv_buf))
    {
        return 0;
    }

    struct chat_message *msg = calloc(1, sizeof(struct chat_message));
    msg->data = peer->recv_buf.buf;
    msg->len = peer->recv_buf.len;
    struct message_peer *mp = calloc(1, sizeof(struct message_peer));
    mp->msg = msg;
    mp->author = peer->socket;

    chat_server_add_message(server, mp);
    recv_buf_free(&peer->recv_buf);
    return 0;
}

static int
send_client_message(struct chat_server *server, struct chat_peer *peer, struct pollfd *pfd)
{
    (void)server;
    char *data;
    int len;
    if (send_queue_get_pending_chunk(&peer->send_queue, &data, &len) == -1)
    {
        /* Нет данных для отправки, возможно ранее забыли убрать флаг готовности отправки */
        pfd->events &= ~POLLOUT;
        return 0;
    }

    int sent = send(peer->socket, data, len, 0);
    if (sent == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 0;
        }

        return -1;
    }

    send_queue_record_sent(&peer->send_queue, sent);
    if (send_queue_empty(&peer->send_queue))
    {
        /* Если больше нечего отправлять, то заканчиваем ожидать возможность отправки на этот сокет */
        pfd->events &= ~POLLOUT;
    }

    return 0;
}

/**
 * @brief Удалить клиента из списка сервера по указанному индексу в массиве pollfd
 *
 * @param fds_idx Индекс элемента в массиве @a s->fds, который надо удалить
 */
static void
chat_server_remove_client(struct chat_server *s, int fds_idx)
{
    assert(fds_idx != 0);
    assert(fds_idx < s->fds_cnt);
    /*
     * Для удаления клиента используем простое перемещение структур - memmove.
     * Но если надо удалить последний элемент, то просто зануляем нужную структуру
     */

    /* fds */
    if (fds_idx != (s->fds_cnt - 1))
    {
        memmove(&s->fds[fds_idx], &s->fds[fds_idx + 1], (s->fds_cnt - fds_idx - 1) * sizeof(*s->fds));
    }

    int last_idx = s->fds_cnt - 1;
    memset(&s->fds[last_idx], 0, sizeof(s->fds[last_idx]));

    /* peers */
    int peer_idx = fds_idx - 1;
    int peer_cnt = s->fds_cnt - 1;

    if (peer_idx != peer_cnt)
    {
        memmove(&s->peers[peer_idx], &s->peers[peer_idx + 1], (peer_cnt - peer_idx - 1) * sizeof(*s->peers));
    }

    last_idx = peer_cnt - 1;
    memset(&s->peers[last_idx], 0, sizeof(s->peers[last_idx]));

    --s->fds_cnt;
}

static bool
socket_has_data_to_read(int sock)
{
    int count;
    int rc = ioctl(sock, FIONREAD, &count);
    assert(rc == 0);
    return 0 < count;
}

int chat_server_update(struct chat_server *server, double timeout)
{
    if (!SERVER_IS_INIT(server))
    {
        return CHAT_ERR_NOT_STARTED;
    }

    if (timeout < 0 && timeout != -1)
    {
        errno = EINVAL;
        return CHAT_ERR_SYS;
    }

    int nfds = poll(server->fds, server->fds_cnt, calc_timeout(timeout));
    if (nfds == 0)
    {
        return CHAT_ERR_TIMEOUT;
    }

    if (nfds == -1)
    {
        return CHAT_ERR_SYS;
    }

    if (SERVER_HAS_ACCEPT(server))
    {
        int new_client = accept(server->socket, NULL, NULL);
        struct pollfd *cpfd = chat_server_add_client(server, new_client);

        /*
         * Если клиент подключился и сразу отправил данные, то после обычного accept мы не заметим этого (в тестах).
         * Поэтому делаем такой хак - если данные есть в буфере, то выставляем revents у соответствующего pollfd клиента.
         * 
         * Также, если данные есть, то nfds не уменьшаем - чтобы корректно прочитать данные клиента
         */
        if (socket_has_data_to_read(new_client))
        {
            cpfd->revents |= POLLIN;
        }
        else
        {
            --nfds;
        }
    }

    struct pollfd *pfd;
    for (int i = 1; i < server->fds_cnt && 0 < nfds;)
    {
        pfd = server->fds + i;
        struct chat_peer *peer = server->peers + (i - 1);
        bool should_delete = false;
        if (CLIENT_READY_READ(pfd))
        {
            if (consume_client_message(server, peer) == -1)
            {
                should_delete = true;
            }
        }

        if (!should_delete && CLIENT_READY_WRITE(pfd))
        {
            if (send_client_message(server, peer, pfd) == -1)
            {
                should_delete = true;
            }
        }

        if (should_delete)
        {
            chat_server_remove_client(server, i);
        }
        else
        {
            i++;
        }

        --nfds;
    }

    return 0;
}

int chat_server_get_descriptor(const struct chat_server *server)
{
#if NEED_SERVER_FEED
    /* IMPLEMENT THIS FUNCTION if want +5 points. */

    /*
     * Server has multiple sockets - own and from connected clients. Hence
     * you can't return a socket here. But if you are using epoll/kqueue,
     * then you can return their descriptor. These descriptors can be polled
     * just like sockets and will return an event when any of their owned
     * descriptors has any events.
     *
     * For example, assume you created an epoll descriptor and added to
     * there a listen-socket and a few client-sockets. Now if you will call
     * poll() on the epoll's descriptor, then on return from poll() you can
     * be sure epoll_wait() can return something useful for some of those
     * sockets.
     */
#endif
    (void)server;
    return -1;
}

int chat_server_get_socket(const struct chat_server *server)
{
    return server->socket;
}

int chat_server_get_events(const struct chat_server *server)
{
    if (!SERVER_IS_INIT(server))
    {
        return 0;
    }

    if (0 < server->msgs_cnt)
    {
        return CHAT_EVENT_INPUT | CHAT_EVENT_OUTPUT;
    }

    return CHAT_EVENT_INPUT;
}

int chat_server_feed(struct chat_server *server, const char *msg, uint32_t msg_size)
{
#if NEED_SERVER_FEED
    /* IMPLEMENT THIS FUNCTION if want +5 points. */
#endif
    (void)server;
    (void)msg;
    (void)msg_size;
    return CHAT_ERR_NOT_IMPLEMENTED;
}
