#ifndef UTILS_H
#define UTILS_H

/// @brief Получить все имена файлов, которые необходимо отсортировать.
/// Дополнительно, этот метод проверяет и возможность работы с файлами (права доступа, файл или директория)
/// @param argc Количество аргументов командной строки
/// @param argv Аргументы командной строки
/// @param count Длина полученного списка
/// @return Список указателей на названия файлов (из @ref argv)
const char **extract_filenames(int argc, const char **argv, int *count);

typedef struct temp_file_struct
{
    char *filename;
    int fd;
} temp_file_t;

/**
 * @brief Создать название временного файла, но не создавать пока
 * 
 * @return const char* Указатель на название временного файла. 
 * Место выделено, поэтому надо вызвать free
 */
void temp_file_init(temp_file_t* temp_file);

void temp_file_free(temp_file_t *temp_file);

#endif