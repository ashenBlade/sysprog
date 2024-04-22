#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include "userfs.h"

enum
{
    BLOCK_SIZE = 512,
    MAX_FILE_SIZE = 1024 * 1024 * 100,
};

/*
 * Проверка на то, что указанное число кратно размеру блока.
 * Необходимо во время нахождения очередного блока при записи
 */
#define IS_MULTIPLE_OF_BLOCKSIZE(size) (((size) & (BLOCK_SIZE - 1)) == 0)

/** Global error code. Set from any function on any error. */
static enum ufs_error_code ufs_error_code = UFS_ERR_NO_ERR;

typedef struct block
{
    /** Block memory. */
    char *data;
    /** How many bytes are occupied. */
    size_t occupied;
    /** Next block in the file. */
    struct block *next;
    /** Previous block in the file. */
    struct block *prev;

    /* PUT HERE OTHER MEMBERS */
} ublock_t;

static void
ublock_init(ublock_t *block)
{
    block->data = (char *)calloc(BLOCK_SIZE, sizeof(char));
    block->occupied = 0;
    block->next = NULL;
    block->prev = NULL;
}

static void
ublock_delete(ublock_t *block)
{
    free(block->data);
    block->data = NULL;
    block->occupied = 0;
    block->next = NULL;
    block->prev = NULL;
}

static size_t
ublock_write(ublock_t *block, const void *data, size_t length)
{
    if (block->occupied == BLOCK_SIZE)
    {
        return 0;
    }

    size_t to_write = BLOCK_SIZE - block->occupied < length
                          ? BLOCK_SIZE - block->occupied
                          : length;
    memcpy(block->data + block->occupied, data, to_write);
    block->occupied += to_write;
    return to_write;
}

typedef struct file
{
    /** Double-linked list of file blocks. */
    ublock_t *block_list;
    /**
     * Last block in the list above for fast access to the end
     * of file.
     */
    ublock_t *last_block;
    /** How many file descriptors are opened on the file. */
    int refs;
    /** File name. */
    char *name;
    /** Files are stored in a double-linked list. */
    struct file *next;
    struct file *prev;

    /** Удален ли файл */
    bool deleted;
    /** Общий размер файла */
    size_t size;
} ufile_t;

static void
ufile_init(ufile_t *file, const char *filename)
{
    file->name = strdup(filename);
    file->block_list = NULL;
    file->last_block = NULL;
    file->next = NULL;
    file->prev = NULL;
    file->refs = 0;
    file->deleted = false;
    file->size = 0;
}

static void
ufile_delete(ufile_t *file)
{
    free(file->name);
    ublock_t *b = file->block_list;
    while (b != NULL)
    {
        ublock_delete(b);
        b = b->next;
    }
    file->block_list = NULL;
    file->last_block = NULL;
    ufile_t *next = file->next,
            *prev = file->prev;
    if (next != NULL)
    {
        next->prev = prev;
    }

    if (prev != NULL)
    {
        prev->next = next;
    }

    file->size = 0;
    file->refs = 0;
    file->deleted = true;
}

static ublock_t *
ufile_get_block_pos(ufile_t *file, size_t pos)
{
    assert(pos < file->size);
    size_t file_blocks_count = file->size / BLOCK_SIZE;
    size_t pos_block_no = pos / BLOCK_SIZE;
    if (file_blocks_count == pos_block_no)
    {
        return file->last_block;
    }
    /*
     * В зависимости от номера необходимого блока начинаем обход списка:
     * возможно, поиск с конца будет быстрее
     */
    ublock_t *target;
    if (pos_block_no <= file_blocks_count / 2)
    {
        /* Начинаем с начала списка */
        target = file->block_list;
        for (size_t i = 0; i < pos_block_no; i++)
        {
            target = target->next;
        }
    }
    else
    {
        /* Начинаем с конца */
        target = file->last_block;
        for (int i = file_blocks_count - pos_block_no; i >= 0; i--)
        {
            target = target->prev;
        }
    }
    return target;
}

static ssize_t
ufile_write(ufile_t *file, size_t pos, const char *data, size_t size)
{
    assert(pos <= file->size && "Проверка позиции должна осуществляться раньше");

    /* Предварительно проверим ограничение на максимальный размер файла */
    size_t new_size = file->size + size;
    if (MAX_FILE_SIZE < new_size)
    {
        ufs_error_code = UFS_ERR_NO_MEM;
        return -1;
    }

    ublock_t *block;
    if (pos == file->size && IS_MULTIPLE_OF_BLOCKSIZE(pos))
    {
        /*
         * Писать надо с конца файла и при этом указанная позиция кратна размеру блока.
         * Это значит, что необходимо начать новый блок.
         */
        ublock_t *new_block = (ublock_t *)calloc(1, sizeof(ublock_t));
        ublock_init(new_block);
        if (file->block_list == NULL)
        {
            file->block_list = new_block;
            file->last_block = new_block;
        }
        else
        {
            new_block->prev = file->last_block;
            file->last_block->next = new_block;
        }

        block = new_block;
    }
    else
    {
        /* Необходимо найти существующий блок по смещению */
        block = ufile_get_block_pos(file, pos);
    }

    /*
     * Шаги:
     * 1. Ищем нужный блок - итерируемся (+ проверяем не NULL)
     * 1.1. Удаляем оставшуюся дальше цепочку
     * 2. В цикле:
     *      1. Пишем часть данных буфера
     *      2. Если конец (left == 0) - выходим из цикла
     *      3. Создаем новый блок и цепляем к концу текущего
     * 3. Обновляем last_block
     */

    size_t written = 0;
    while (written < size)
    {
        size_t cur_written = ublock_write(block, data + written, size - written);
        written += cur_written;
        if (written < size || cur_written == 0)
        {
            ublock_t *next = block->next;
            if (next == NULL)
            {
                next = (ublock_t *)calloc(1, sizeof(ublock_t));
                ublock_init(next);
                next->prev = block;
                block->next = next;
                file->last_block = next;
            }
            else
            {
                block = next;
            }
        }
    }

    file->size += size;
    return (ssize_t) written;
}

/** List of all files. */
static ufile_t *ufile_list = NULL;

typedef struct filedesc
{
    ufile_t *file;
    size_t pos;
} ufd_t;

static void
ufd_init(ufd_t *fd, ufile_t *file)
{
    ++file->refs;
    fd->file = file;
    fd->pos = 0;
}

static void
ufd_close(ufd_t *ufd)
{
    int left_refs = --ufd->file->refs;

    if (left_refs == 0 && ufd->file->deleted)
    {
        ufile_delete(ufd->file);
    }

    ufd->file = NULL;
    ufd->pos = 0;
}

static ssize_t
ufd_write(ufd_t *ufd, const char *data, size_t size)
{
    /* Проверка на изменение размера файла */
    if (ufd->file->size < ufd->pos)
    {
        ufd->pos = ufd->file->size;
    }

    ssize_t written = ufile_write(ufd->file, ufd->pos, data, size);
    if (written == -1)
    {
        return -1;
    }

    ufd->pos += written;
    return written;
}

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
#define INITIAL_FD_LIST_CAPACITY 32

static ufd_t **ufds = NULL;
static int ufds_count = 0;
static int ufds_capacity = 0;

enum ufs_error_code
ufs_errno()
{
    return ufs_error_code;
}

static ufile_t *
search_existing_file(const char *filename)
{
    if (ufile_list == NULL)
    {
        return NULL;
    }

    struct file *cur = ufile_list;
    do
    {
        if (strcmp(cur->name, filename) == 0 && !cur->deleted)
        {
            return cur;
        }
        cur = cur->next;
    } while (cur != NULL);
    return NULL;
}

static void
file_list_add(ufile_t *file)
{
    if (ufile_list == NULL)
    {
        ufile_list = file;
        return;
    }

    ufile_t *cur = ufile_list;
    while (cur->next != NULL)
    {
        cur = cur->next;
    }

    file->prev = cur;
    cur->next = file;
}

static int
create_file_desc(ufile_t *file)
{
    assert(file != NULL);

    int fd = -1;
    if (ufds_capacity == 0)
    {
        /* Изначально массив дескрипторов не инициализирован */
        ufds = (ufd_t **)calloc(INITIAL_FD_LIST_CAPACITY, sizeof(ufd_t *));
        ufds_capacity = INITIAL_FD_LIST_CAPACITY;
        ++ufds_count;
        fd = 0;
    }
    else
    {
        /* Находим первый свободный слот */
        for (int i = 0; i < ufds_count; i++)
        {
            if (ufds[i] == NULL)
            {
                fd = i;
                break;
            }
        }

        /* Если не нашли, то берем последний + проверяем вместимость массива */
        if (fd == -1)
        {
            if (ufds_capacity == ufds_count)
            {
                ufds_capacity *= 2;
                ufds = (ufd_t **)realloc(ufds, ufds_capacity * sizeof(ufd_t *));
            }

            fd = ufds_count;
            ++ufds_count;
        }
    }

    ufd_t *ufd = (ufd_t *)calloc(1, sizeof(ufd_t));
    ufd_init(ufd, file);
    ufds[fd] = ufd;
    return fd;
}

int ufs_open(const char *filename, int flags)
{
    /*
     * 1. Нахождение файла по его имени среди всех
     * 2. Создание при необходимости (flags)
     * 3. Получение дескриптора (создание)
     * 4. Увеличение счетчика указателей на файл
     */

    ufile_t *file = search_existing_file(filename);
    if (file == NULL)
    {
        if (flags && UFS_CREATE)
        {
            file = (ufile_t *)calloc(1, sizeof(ufile_t));
            ufile_init(file, filename);
            file_list_add(file);
        }
        else
        {
            ufs_error_code = UFS_ERR_NO_FILE;
            return -1;
        }
    }

    return create_file_desc(file);
}

static ufd_t *
search_ufd(int fd)
{
    if (fd < 0 || ufds_count <= fd)
    {
        return NULL;
    }

    return ufds[fd];
}

ssize_t
ufs_write(int fd, const char *buf, size_t size)
{
    ufd_t *ufd = search_ufd(fd);
    if (ufd == NULL || buf == NULL)
    {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }

    if (size == 0)
    {
        return 0;
    }

    return ufd_write(ufd, buf, size);
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{
    /* IMPLEMENT THIS FUNCTION */
    (void)fd;
    (void)buf;
    (void)size;
    ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;
    return -1;
}

int ufs_close(int fd)
{
    ufd_t *ufd = search_ufd(fd);
    if (ufd == NULL)
    {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }

    ufd_close(ufd);
    ufds[fd] = NULL;
    free((void *)ufd);
    return 0;
}

static ufile_t *
search_ufile(const char *filename)
{
    ufile_t *cur = ufile_list;
    while (cur != NULL)
    {
        if (strcmp(cur->name, filename) == 0)
        {
            return cur;
        }
        cur = cur->next;
    }

    return NULL;
}

int ufs_delete(const char *filename)
{
    if (filename == NULL)
    {
        return -1;
    }

    ufile_t *file = search_ufile(filename);
    if (file->deleted)
    {
        /* Уже удален */
        return 0;
    }

    file->deleted = true;
    ufile_t *next = file->next,
            *prev = file->prev;
    if (next != NULL)
    {
        next->prev = prev;
    }

    if (prev != NULL)
    {
        prev->next = next;
    }

    if (file->refs == 0)
    {
        ufile_delete(file);
        free(file);
    }

    return 0;
}

void ufs_destroy(void)
{
}
