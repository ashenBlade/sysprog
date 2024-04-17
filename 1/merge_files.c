#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>

#include "merge_files.h"
#include "priority_queue.h"

typedef struct page_file_writer_state
{
    char *chunk;
    int size;
    int capacity;
    int fd;
} page_writer_t;

static void page_writer_init(page_writer_t *writer, int fd, int capacity)
{
    writer->chunk = (char *)malloc(sizeof(char) * capacity);
    writer->fd = fd;
    writer->capacity = capacity;
    writer->size = 0;
}

static void page_writer_free(page_writer_t *writer)
{
    free(writer->chunk);
    writer->capacity = 0;
    writer->size = 0;
}

static void page_writer_flush(page_writer_t *writer)
{
    if (writer->size == 0)
    {
        return;
    }

    int left = writer->size;
    int pos = 0;
    while (0 < left)
    {
        int written = write(writer->fd, writer->chunk + pos, left);
        if (written == -1)
        {
            perror("write");
            exit(1);
        }

        left -= written;
        pos += written;
    }

    writer->size = 0;
}

static void page_writer_write(page_writer_t *writer, int number)
{
    char buf[11 /* 10 (цифр в числе макс) + 1 (пробел) */];
    int buf_size = snprintf(buf, 11, "%d", number);
    buf[buf_size] = ' ';
    ++buf_size;
    int chunk_left = writer->capacity - writer->size;

    if (buf_size <= chunk_left)
    {
        /* Если есть свободное место в буфере, то просто копируем и возвращаемся */
        memcpy(writer->chunk + writer->size, buf, buf_size);
        writer->size += buf_size;
        return;
    }

    /*
     * 1. Заполняем оставшееся место в чанке
     * 2. Сбрасываем на диск
     * 3. Копируем оставшееся из буфера в чанк
     */

    int to_write = writer->capacity - writer->size;
    memcpy(writer->chunk + writer->size, buf, to_write);
    writer->size += to_write;
    page_writer_flush(writer);

    int left_buf_size = buf_size - to_write;
    memcpy(writer->chunk, buf + to_write, left_buf_size);
    writer->size = left_buf_size;
}

typedef struct page_reader
{
    int fd;
    char *chunk;
    int capacity;
    int size;
    int pos;
    bool eof;
} page_reader;

static void page_reader_init(page_reader *reader, int fd, int capacity)
{
    reader->fd = fd;
    reader->chunk = (char *)malloc(sizeof(char) * capacity);
    reader->capacity = capacity;
    reader->size = 0;
    reader->pos = 0;
    reader->eof = false;
}

static void page_reader_delete(page_reader *reader)
{
    free(reader->chunk);
    reader->fd = -1;
    reader->chunk = NULL;
    reader->capacity = 0;
    reader->size = 0;
    reader->pos = 0;
    reader->eof = true;
}

#define IS_4_MULTIPLE(x) (((x) & 0b11) == 0)

static bool page_reader_try_read_next_chunk(page_reader *reader)
{
    assert(reader->pos == reader->size);
    assert(!reader->eof);

    /* При чтении нужно учитывать, что размер чанка должен быть кратен 4 - размер int */

    int size = 0;
    do
    {
        int current_read = read(reader->fd, reader->chunk + size, reader->capacity - size);
        if (current_read == -1)
        {
            perror("read");
            exit(1);
        }

        size += current_read;
    } while (!IS_4_MULTIPLE(size) && reader->capacity < size);

    if (size == 0)
    {
        reader->eof = true;
        return false;
    }

    reader->pos = 0;
    reader->size = size;
    return true;
}

static bool page_reader_try_read_number(page_reader *reader, int *number)
{
    if (reader->eof)
    {
        return false;
    }

    if (reader->pos == reader->size)
    {
        if (!page_reader_try_read_next_chunk(reader))
        {
            return false;
        }
    }

    *number = *(int *)(reader->chunk + reader->pos);
    reader->pos += sizeof(int);
    return true;
}

typedef struct merge_files_state
{
    page_reader *readers;
    int count;
    priority_queue_t pq;
} merge_state;

static void merge_state_init(merge_state *state, int *fds, int count)
{
    priority_queue_init(&state->pq);
    state->count = count;
    page_reader *readers = (page_reader *)malloc(sizeof(page_reader) * count);
    for (long i = 0; i < count; i++)
    {
        page_reader *reader = &readers[i];
        page_reader_init(reader, fds[i], 4096);
        int number;
        if (page_reader_try_read_number(reader, &number))
        {
            priority_queue_enqueue(&state->pq, number, (void *)reader);
        }
    }
    state->readers = readers;
}

static void merge_state_free(merge_state *state)
{
    for (long i = 0; i < state->count; i++)
    {
        page_reader_delete(&state->readers[i]);
    }
    free(state->readers);
    priority_queue_delete(&state->pq);
    state->count = 0;
}

static bool merge_state_try_read_next_number(merge_state *state, int *number)
{
    void *ptr = NULL;
    if (priority_queue_try_dequeue(&state->pq, number, &ptr))
    {
        page_reader *reader = (page_reader *)ptr;
        int next_number;
        if (page_reader_try_read_number(reader, &next_number))
        {
            priority_queue_enqueue(&state->pq, next_number, reader);
        }

        return true;
    }
    return false;
}

void merge_files(int result_fd, int *fds, int count)
{
    merge_state state;
    merge_state_init(&state, fds, count);

    page_writer_t writer;
    page_writer_init(&writer, result_fd, 4096);

    int number;
    while (merge_state_try_read_next_number(&state, &number))
    {
        page_writer_write(&writer, number);
    }

    page_writer_flush(&writer);

    page_writer_free(&writer);
    merge_state_free(&state);
}