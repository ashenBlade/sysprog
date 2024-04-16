#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include "external_sort.h"
#include "libcoro.h"
#include "number_file_reader.h"
#include "insertion_sort.h"

static void read_and_sort_coro(int src_fd, insertion_sort_state *is, int max_memory_bytes)
{
    file_read_state *read_state = file_read_state_new(src_fd, max_memory_bytes);
    
    int number;
    yield();
    while (file_read_state_get_next_number(read_state, &number))
    {
        insertion_sort_insert(is, number);
        yield();
    }

    file_read_state_delete(read_state);
}

static void save_to_temp_file_coro(insertion_sort_state *is, int fd)
{
    char *buffer = (char*) insertion_sort_array(is);
    int array_size = insertion_sort_size(is);
    int left = array_size * sizeof(int);
    int pos = 0;
    while (0 < left)
    {
        int written = write(fd, buffer + pos, left);
        if (written == -1)
        {
            perror("write");
            exit(1);
        }

        pos += written;
        left -= written;
        yield();
    }
}

static int get_chunk_read_size()
{
    int result = (int) sysconf(_SC_PAGESIZE); 
    if (result == -1)
    {
        return 4096 /* Размер по умолчанию, думаю на большинстве систем такое значение */;
    }

    return result;
}

void sort_file_external_coro(int src_fd, int temp_fd)
{
    insertion_sort_state sorted_state;
    insertion_sort_init(&sorted_state);

    /* Вначале читаем файл и одновременно строим сортированный массив */
    read_and_sort_coro(src_fd, &sorted_state, get_chunk_read_size());
    
    /* После сохраняем готовый сортированный массив в результирующий файл */
    save_to_temp_file_coro(&sorted_state, temp_fd);

    insertion_sort_free(&sorted_state);
}
