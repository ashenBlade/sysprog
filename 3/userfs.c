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

/** Global error code. Set from any function on any error. */
static enum ufs_error_code ufs_error_code = UFS_ERR_NO_ERR;

struct block
{
    /** Block memory. */
    char *memory;
    /** How many bytes are occupied. */
    int occupied;
    /** Next block in the file. */
    struct block *next;
    /** Previous block in the file. */
    struct block *prev;

    /* PUT HERE OTHER MEMBERS */
};

typedef struct file
{
    /** Double-linked list of file blocks. */
    struct block *block_list;
    /**
     * Last block in the list above for fast access to the end
     * of file.
     */
    struct block *last_block;
    /** How many file descriptors are opened on the file. */
    int refs;
    /** File name. */
    char *name;
    /** Files are stored in a double-linked list. */
    struct file *next;
    struct file *prev;

    /* PUT HERE OTHER MEMBERS */
} ufile_t;

/** List of all files. */
static ufile_t *file_list = NULL;

typedef struct filedesc
{
    ufile_t *file;

    /* PUT HERE OTHER MEMBERS */
    int pos;
} ufd_t;

static void
filedesc_init(ufd_t *fd, ufile_t *file)
{
    fd->file = file;
    fd->pos = 0;
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
    if (file_list == NULL)
    {
        return NULL;
    }

    struct file *cur = file_list;
    do
    {
        if (strcmp(cur->name, filename) == 0)
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
    if (file_list == NULL)
    {
        file_list = file;
        return;
    }

    assert(strcmp(file->name, file_list->name) != 0 && "Проверка существования файла должна быть произведена выше");

    ufile_t *cur = file_list;
    while (cur->next != NULL)
    {
        cur = cur->next;
        assert(strcmp(file->name, cur->name) != 0 && "Проверка существования файла должна быть произведена выше");
    }

    file->prev = cur;
    cur->next = file;
}

static ufile_t *
create_file(const char *filename)
{
    ufile_t *file = (ufile_t *)calloc(1, sizeof(ufile_t));
    file->name = strdup(filename);
    file->block_list = NULL;
    file->last_block = NULL;
    file->next = NULL;
    file->prev = NULL;
    file->refs = 0;

    file_list_add(file);

    return file;
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
        if (fd == -1)
        {
            /* Если не нашли, то берем последний + проверяем вместимость массива */
            if (ufds_capacity == ufds_count)
            {
                ufds_capacity *= 2;
                ufds = (ufd_t **)realloc(ufds, ufds_capacity * sizeof(ufd_t *));
            }

            fd = ufds_count;
        }
    }

    ufd_t *ufd = (ufd_t *)calloc(1, sizeof(ufd_t));
    filedesc_init(ufd, file);
    ufds[fd] = ufd;
    return fd;
}

int ufs_open(const char *filename, int flags)
{
    /* IMPLEMENT THIS FUNCTION */
    (void)filename;
    (void)flags;
    (void)file_list;
    (void)ufds;
    (void)ufds_count;
    (void)ufds_capacity;
    ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;
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
            file = create_file(filename);
        }
        else
        {
            ufs_error_code = UFS_ERR_NO_FILE;
            return -1;
        }
    }

    return create_file_desc(file);
}

ssize_t
ufs_write(int fd, const char *buf, size_t size)
{
    /* IMPLEMENT THIS FUNCTION */
    (void)fd;
    (void)buf;
    (void)size;
    ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;
    return -1;
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
    /* IMPLEMENT THIS FUNCTION */
    (void)fd;
    ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;
    return -1;
}

int ufs_delete(const char *filename)
{
    /* IMPLEMENT THIS FUNCTION */
    (void)filename;
    ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;
    return -1;
}

void ufs_destroy(void)
{
}
