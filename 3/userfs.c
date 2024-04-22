#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include "userfs.h"

enum ufs_config
{
    BLOCK_SIZE = 512
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

static ublock_t *
ublock_new()
{
    ublock_t *block = (ublock_t *)calloc(1, sizeof(ublock_t));
    ublock_init(block);
    return block;
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
ublock_write(ublock_t *block, size_t pos, const char *data, size_t length)
{
    assert(pos <= block->occupied);
    if (pos == BLOCK_SIZE)
    {
        return 0;
    }

    size_t to_write = BLOCK_SIZE - pos < length
                          ? BLOCK_SIZE - pos
                          : length;
    memcpy(block->data + pos, data, to_write);

    if (block->occupied < pos + to_write)
    {
        block->occupied = pos + to_write;
    }

    return to_write;
}

static size_t
ublock_read(ublock_t *block, size_t pos, char *buf, size_t length)
{
    assert(pos <= block->occupied);
    if (pos == BLOCK_SIZE)
    {
        return 0;
    }

    size_t to_read = block->occupied - pos < length
                         ? block->occupied - pos
                         : length;
    memcpy(buf, block->data + pos, to_read);
    return to_read;
}

#ifdef NEED_RESIZE

static void
ublock_resize(ublock_t *block, size_t size)
{
    assert(size <= BLOCK_SIZE);
    if (block->occupied == size)
    {
        return;
    }

    char *start;
    size_t length;
    if (size < block->occupied)
    {
        start = block->data + size;
        length = block->occupied - size;
    }
    else
    {
        start = block->data + block->occupied;
        length = size - block->occupied;
    }

    memset((void *)start, 0, length);
    block->occupied = size;
}

#endif

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
    file->size = 0;
    file->refs = 0;
    file->deleted = true;
}

static ublock_t *
ufile_get_block_pos(ufile_t *file, size_t pos)
{
    assert(pos <= file->size);

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
    if (UFS_CONSTR_MAX_FILE_SIZE < pos + size)
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
        ublock_t *new_block = ublock_new();
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

    size_t written = 0;
    /* Вначале, записываем в конец блока, а после - начинаем с начала */
    size_t ublock_pos = pos % BLOCK_SIZE;
    while (written < size)
    {
        size_t cur_written = ublock_write(block, ublock_pos, data + written, size - written);
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

            block = next;
            /* Запись в каждый следующий блок начинаем с начала */
            ublock_pos = 0;
        }
    }

    if (file->size < (pos + size))
    {
        file->size = pos + size;
    }

    return (ssize_t)written;
}

/** List of all files. */
static ufile_t *ufile_list = NULL;

/**
 * Выполнить полное удаление файла.
 * Замечание: все файловые дескрипторы должны быть закрыты
 */
static void
ufile_list_remove(ufile_t *file)
{
    assert(file->refs == 0);
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

    if (file == ufile_list)
    {
        ufile_list = next;
    }

    ufile_delete(file);
    free(file);
}

static ufile_t *
ufile_list_search_existing(const char *filename)
{
    ufile_t *file = ufile_list;
    while (file != NULL)
    {
        if (strcmp(file->name, filename) == 0 && !file->deleted)
        {
            return file;
        }
        file = file->next;
    }

    return NULL;
}

#ifdef NEED_RESIZE

static int
ufile_resize(ufile_t *file, size_t size)
{
    if (file->size == size)
    {
        return 0;
    }

    if (UFS_CONSTR_MAX_FILE_SIZE < size)
    {
        ufs_error_code = UFS_ERR_NO_MEM;
        return -1;
    }

    if (file->size < size)
    {
        if (file->block_list == NULL)
        {
            ublock_t *first_block = ublock_new();
            file->block_list = first_block;
            file->last_block = first_block;
        }

        int fill_blocks_count = file->size / BLOCK_SIZE;
        ublock_t *last_block = file->last_block;

        for (size_t i = 0; i < fill_blocks_count; i++)
        {
            ublock_resize(last_block, BLOCK_SIZE);
            ublock_t *next_block = ublock_new();
            next_block->prev = last_block;
            last_block->next = next_block;
            last_block = next_block;
        }

        size_t last_block_size = size % BLOCK_SIZE;
        if (last_block_size != 0)
        {
            ublock_t *new_last_block = ublock_new();
            ublock_resize(new_last_block, last_block_size);
            new_last_block->prev = last_block;
            last_block->next = new_last_block;
        }

        file->last_block = last_block;
        file->size = size;
    }
    else /* size < file->size */
    {
        ublock_t *new_last_block = ufile_get_block_pos(file, size);
        ublock_resize(new_last_block, size % BLOCK_SIZE);

        ublock_t *block = new_last_block->next;
        while (block != NULL)
        {
            ublock_t *next = block->next;
            ublock_delete(block);
            free(block);
            block = next;
        }

        new_last_block->next = NULL;
        file->last_block = new_last_block;
        file->size = size;
    }

    return 0;
}

#endif

typedef struct filedesc
{
    /** Указатель на рабочий файл */
    ufile_t *file;
    /** Позиция в файле, с которой мы работаем */
    size_t pos;
    /** Флаги разрешений */
    enum open_flags flags;
} ufd_t;

#ifdef NEED_OPEN_FLAGS

#define can_write(flags) (((flags) & UFS_WRITE_ONLY) != 0)
#define can_read(flags) (((flags) & UFS_READ_ONLY) != 0)
#define setup_rw_permissions(flags) (((flags) & UFS_READ_WRITE) == 0 ? (UFS_READ_WRITE | (flags)) : (flags))

#else

#define can_write(flags) (true)
#define can_read(flags) (true)
#define setup_rw_permissions(flags) (flags)

#endif

static void
ufd_init(ufd_t *fd, ufile_t *file, enum open_flags flags)
{
    ++file->refs;
    fd->file = file;
    fd->pos = 0;
    fd->flags = setup_rw_permissions(flags);
}

static void
ufd_close(ufd_t *ufd)
{
    int left_refs = --ufd->file->refs;

    if (left_refs == 0 && ufd->file->deleted)
    {
        ufile_list_remove(ufd->file);
    }

    ufd->file = NULL;
    ufd->pos = 0;
}

/* Проверка на изменение размера файла */
static void
ufd_adjust_pos(ufd_t *ufd)
{
    if (ufd->file->size < ufd->pos)
    {
        ufd->pos = ufd->file->size;
    }
}

static ssize_t
ufd_write(ufd_t *ufd, const char *data, size_t size)
{
#ifdef NEED_OPEN_FLAGS
    if (!can_write(ufd->flags))
    {
        ufs_error_code = UFS_ERR_NO_PERMISSION;
        return -1;
    }
#endif

    ufd_adjust_pos(ufd);

    ssize_t written = ufile_write(ufd->file, ufd->pos, data, size);
    if (written == -1)
    {
        return -1;
    }

    ufd->pos += written;
    return written;
}

static size_t
ufd_read(ufd_t *ufd, char *buf, size_t length)
{
#ifdef NEED_OPEN_FLAGS
    if (!can_read(ufd->flags))
    {
        ufs_error_code = UFS_ERR_NO_PERMISSION;
        return -1;
    }
#endif

    if (length == 0)
    {
        return 0;
    }

    ufd_adjust_pos(ufd);

    if (ufd->file->size == ufd->pos)
    {
        /* Конец файла */
        return 0;
    }

    ublock_t *block = ufile_get_block_pos(ufd->file, ufd->pos);
    size_t read = 0;
    size_t ublock_pos = ufd->pos % BLOCK_SIZE;
    do
    {
        size_t cur_read = ublock_read(block, ublock_pos, buf + read, length - read);
        read += cur_read;
        block = block->next;
        ublock_pos = 0;
    } while (read < length && block != NULL);

    ufd->pos += read;
    return read;
}

#ifdef NEED_RESIZE

static int
ufd_resize(ufd_t *ufd, size_t size)
{
#ifdef NEED_OPEN_FLAGS
    if (!can_write(ufd->flags))
    {
        ufs_error_code = UFS_ERR_NO_PERMISSION;
        return -1;
    }
#endif

    if (ufile_resize(ufd->file, size) == -1)
    {
        return -1;
    }

    if (size < ufd->pos)
    {
        ufd->pos = size;
    }
    
    return 0;
}

#endif

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

static void
ufile_list_add(ufile_t *file)
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
create_file_desc(ufile_t *file, enum open_flags flags)
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
    ufd_init(ufd, file, flags);
    ufds[fd] = ufd;
    return fd;
}

int ufs_open(const char *filename, int flags)
{
    /* Находим первый не удаленный файл */
    ufile_t *file = ufile_list_search_existing(filename);

    /* Создаем новый при необходимости */
    if (file == NULL)
    {
        if (flags & UFS_CREATE)
        {
            file = (ufile_t *)calloc(1, sizeof(ufile_t));
            ufile_init(file, filename);
            ufile_list_add(file);
        }
        else
        {
            ufs_error_code = UFS_ERR_NO_FILE;
            return -1;
        }
    }

    return create_file_desc(file, (enum open_flags)flags);
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
    ufd_t *ufd = search_ufd(fd);
    if (ufd == NULL)
    {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }

    return ufd_read(ufd, buf, size);
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

#ifdef NEED_RESIZE

int ufs_resize(int fd, size_t new_size)
{
    ufd_t *ufd = search_ufd(fd);
    if (ufd == NULL)
    {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }

    return ufd_resize(ufd, new_size);
}

#endif

int ufs_delete(const char *filename)
{
    if (filename == NULL)
    {
        return -1;
    }

    ufile_t *file = ufile_list_search_existing(filename);
    if (file->deleted)
    {
        /* Уже удален */
        return 0;
    }

    file->deleted = true;
    if (file->refs == 0)
    {
        ufile_list_remove(file);
    }
    return 0;
}

void ufs_destroy(void)
{
}
