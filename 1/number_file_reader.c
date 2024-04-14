#include "number_file_reader.h"

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

typedef struct file_read_state
{
    /// @brief Дескриптор файла, из которого мы читаем данные
    int fd;
    /// @brief Буфер с прочитанными данными
    char *buf;
    /// @brief Максимальный размер буфера
    int max_size;
    /// @brief Текущий размер буфера
    int size;
    /// @brief Текущая позиция в буфере, откуда читаем данные
    int pos;
    /// @brief Достигнут ли конец файла
    bool eof;
} file_read_state;

/// @brief Достигнут ли конец файла
#define IS_EOF(state) ((state)->eof)

/// @brief Достигнут ли конец текущего чанка
#define IS_END_BUFFER(state) ((state)->size <= (state)->pos)

/// @brief Получить текущий символ, без увеличения длины
#define CUR_CHAR(state) ((state)->buf[(state)->pos])

file_read_state* file_read_state_new(int fd, int buffer_size)
{
    if (buffer_size <= 0)
    {
        return NULL;
    }

    file_read_state *state = (file_read_state*) malloc(sizeof(file_read_state));
    state->fd = fd;
    state->buf = (char*) malloc(buffer_size);
    state->max_size = buffer_size;
    state->size = 0;
    state->pos = 0;
    state->eof = false;
    return state;
}

void file_read_state_delete(file_read_state *state)
{
    free(state->buf);
    state->pos = 0;
    state->eof = true;
    state->size = 0;
    state->max_size = 0;
    state->fd = -1;
    free(state);
}

static void read_next_chunk(file_read_state *state)
{
    assert(!state->eof);

    /*
     * Переносим оставшиеся данные в начало буфера и размер
     */
    int left = state->size - state->pos;
    if (left != 0)
    {
        memcpy(state->buf, state->buf + state->pos, left);
        state->pos = left;
    }
    else
    {
        state->pos = 0;
    }

    /*
     * Заполняем оставшееся место в буфере (читаем оставшееся)
     */
    int to_read = state->max_size - left;
    assert(to_read + left <= state->max_size);
    int read_count = read(state->fd, state->buf + left, to_read);
    if (read_count == -1)
    {
        perror("read");
        exit(1);
    }

    if (read_count == 0)
    {
        state->eof = true;
    }

    state->size = left + read_count;
}

static bool skip_whitespaces(file_read_state *state)
{
    while (!IS_EOF(state))
    {
        /* Читаем новый чанк, если достигли конца текущего */
        if (state->size <= state->pos)
        {
            read_next_chunk(state);
            continue;
        }

        /* Проходимся по всему буферу, пока не дойдем до конца */
        while (!IS_END_BUFFER(state))
        {
            if (!isspace(CUR_CHAR(state)))
            {
                return true;
            }

            /* Текущий символ - пустая строка, переходим к следующему */
            state->pos++;
        }
    }

    return false;
}

bool file_read_state_get_next_number(file_read_state *state, int *read_number)
{
    /*
     * Пропускаем пустые строки с учетом:
     * - Их может быть несколько
     * - Придется прочитать следующий чанк из файла
     * - Файл закончится
     */
    if (!skip_whitespaces(state))
    {
        return false;
    }

    /*
     * При чтении чанка, число могло быть обрезано.
     * Поэтому вначале находим конец этого числа:
     * - Если это пустая строка - то парсим число и возвращаем
     * - Если достигнут конец буфера - читаем следующий чанк и повторяем
     */
    while (!IS_EOF(state))
    {
        int delta = 1;
        while (state->pos + delta < state->size)
        {
            if (isspace(*(state->buf + delta)))
            {
                /* В условии сказано, что числа укладываются в int */
                int number = (int)strtol(state->buf + state->pos,
                                         NULL /* Адрес не передаем, т.к. вычислили сами */,
                                         10);
                state->pos += delta;
                *read_number = number;
                return true;
            }

            delta++;
        }

        read_next_chunk(state);
        if (IS_EOF(state))
        {
            /*
             * Если достигли конца файла, то последнее число - не обрезано.
             * Возвращаем его и
             */

            int number = (int)strtol(state->buf + state->pos,
                                     NULL,
                                     10);
            state->pos += delta;
            *read_number = number;
            return true;
        }
    }

    return false;
}
