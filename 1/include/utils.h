#ifndef UTILS_H
#define UTILS_H

typedef struct program_args
{
    /** Названия файлов, которые необходимо обработать */
    const char **filenames;
    /** Количество этих файлов в массиве */
    int files_count;
    /** Указанная задержка, либо 0, если не указана */
    long long latency_us;
    /** Количество корутин в пуле, которое нужно использовать */
    int coro_count;
} prog_args_t;

/// @brief Получить все имена файлов, которые необходимо отсортировать.
/// Дополнительно, этот метод проверяет и возможность работы с файлами (права доступа, файл или директория)
/// @param argc Количество аргументов командной строки
/// @param argv Аргументы командной строки
/// @param prog_args Полученные параметры
/// @remarks В результате выделяется массив, который необходимо освободить
void 
extract_program_args(int argc, const char **argv, prog_args_t *prog_args);

typedef struct temp_file_struct temp_file_t;

/**
 * @brief Создать название временного файла, но не создавать пока
 *
 * @return const char* Указатель на название временного файла.
 * Место выделено, поэтому надо вызвать free
 */
temp_file_t *temp_file_new();

/**
 * @brief Получить дескриптор для указанного временного файла
 *
 * @param temp_file
 */
int temp_file_fd(temp_file_t *temp_file);
void temp_file_free(temp_file_t *temp_file);

#endif