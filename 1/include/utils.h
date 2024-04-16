#ifndef UTILS_H
#define UTILS_H

/// @brief Получить все имена файлов, которые необходимо отсортировать.
/// Дополнительно, этот метод проверяет и возможность работы с файлами (права доступа, файл или директория)
/// @param argc Количество аргументов командной строки
/// @param argv Аргументы командной строки
/// @param count Длина полученного списка
/// @remarks В результате выделяется массив, который необходимо освободить 
/// @return Список указателей на названия файлов (из @ref argv)
const char **extract_filenames(int argc, const char **argv, int *count);

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